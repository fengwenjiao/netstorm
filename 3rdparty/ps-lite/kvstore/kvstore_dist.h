/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * Copyright (c) 2015 by Contributors
 * @file   kvstore_dist.h
 * @brief  distributed implementation based on ps-lite
 */
#ifndef MXNET_KVSTORE_KVSTORE_DIST_H_
#define MXNET_KVSTORE_KVSTORE_DIST_H_
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <utility>
#include <functional>
#include <future>
#include "./kvstore_local.h"
#include "mxnet/engine.h"
#include "ps/ps.h"
#include "./kvstore_dist_server.h"
namespace mxnet {
    namespace kvstore {

/**
 * \brief distributed kvstore
 *
 * it's the server node's job to control the data consistency among all
 * workers. see details on \ref ServerHandle::Start
 */
        class KVStoreDist : public KVStoreLocal {
        public:
            explicit KVStoreDist(bool use_device_comm)
                    : KVStoreLocal(use_device_comm), ps_worker_(nullptr), server_(nullptr) {
              if (IsWorkerNode()) {
                int new_customer_id = GetNewCustomerId();
                ps_worker_ = new ps::KVWorker<char>(0, new_customer_id);
                ps::StartAsync(new_customer_id, "mxnet\0");
                using namespace std::placeholders;
                ps_worker_->set_request_handle(
                        std::bind(&KVStoreDist::TaskDivisionUpdate, this, _1));
                if (!ps::Postoffice::Get()->is_recovery()) {
                  ps::Postoffice::Get()->Barrier(
                          new_customer_id,
                          ps::kWorkerGroup + ps::kServerGroup + ps::kScheduler);
                }
              }
              bigarray_bound_ = dmlc::GetEnv("MXNET_KVSTORE_BIGARRAY_BOUND", 1000 * 1000);
              enable_original = dmlc::GetEnv("ENABLE_ORIGINAL", 0);
              log_verbose_ = dmlc::GetEnv("MXNET_KVSTORE_DIST_ROW_SPARSE_VERBOSE", false);
            }

            virtual ~KVStoreDist() {
              Engine::Get()->WaitForAll();
              customer_id_ = 0;
              if (IsWorkerNode()) {
                if (barrier_before_exit_) {
                  Barrier();
                  if (get_rank() == 0 && ps_worker_->get_customer()->customer_id() == 0) {
                    // stop the executor at servers
                    SendCommandToServers(static_cast<int>(CommandType::kStopServer), "");
                  }
                }
                ps::Finalize(ps_worker_->get_customer()->customer_id(), barrier_before_exit_);
                delete ps_worker_;
              }
            }

            void set_updater(const Updater& updater) override {
              LOG(INFO)<<"in worker kvstore set updater";
              CHECK(updater) << "invalid updater";
              if (IsServerNode()) {
                LOG(INFO)<<"5000";
                CHECK_NOTNULL(server_)->set_updater(updater);
              } else {
                LOG(INFO)<<"5001";
                updater_ = updater;
              }
            }

            void SetGradientCompression(const std::vector<std::pair<std::string, std::string> >
                                        & kwargs) override {
              KVStoreLocal::SetGradientCompression(kwargs);
              if (get_rank() == 0) {
                SendCommandToServers(static_cast<int>(CommandType::kSetGradientCompression),
                                     gradient_compression_->EncodeParams());
              }
            }

            void SetServerProfilerCommand(const KVStoreServerProfilerCommand type,
                                          const std::string& params) override {
              if (get_rank() == 0) {
                SendCommandToServers(static_cast<int>(CommandType::kSetProfilerParams),
                                     params + std::to_string(static_cast<int>(type)));
              }
            }


            void Barrier() override {
              ps::Postoffice::Get()->Barrier(ps_worker_->get_customer()->customer_id(), ps::kWorkerGroup);
            }

            void SendCommandToServers(int cmd_id,
                                      const std::string& cmd_body) override {
              CHECK_NOTNULL(ps_worker_);
              ps_worker_->Wait(ps_worker_->Request(cmd_id, cmd_body, ps::kServerGroup));
            }

            int get_group_size() const override { return ps::NumWorkers(); }

            int get_rank() const override { return ps::MyRank(); }

            int get_num_dead_node(int node_id, int timeout) const override {
              int number = 0;
              auto dead_nodes = ps::Postoffice::Get()->GetDeadNodes(timeout);
              const auto& watch_nodes = ps::Postoffice::Get()->GetNodeIDs(node_id);
              std::unordered_set<int> watch_set(watch_nodes.begin(), watch_nodes.end());
              for (int r : dead_nodes) {
                if (watch_set.find(r) != watch_set.end()) number++;
              }
              return number;
            }

            void RunServer(const Controller& controller) override {
              CHECK(!IsWorkerNode());
              if (IsServerNode()) {
                server_ = new KVStoreDistServer();
                server_->set_controller(controller);
              }

              ps::StartAsync(0, "mxnet_server\0");
              if (!ps::Postoffice::Get()->is_recovery()) {
                ps::Postoffice::Get()->Barrier(0,
                                               ps::kWorkerGroup + ps::kServerGroup + ps::kScheduler);
              }
              if (server_) server_->Run();
              ps::Finalize(0, true);
              if (server_) {
                delete server_;
              }
              server_ = nullptr;
            }

        private:
            static std::atomic<int> customer_id_;

            static int GetNewCustomerId() {
              return customer_id_++;
            }


            /**
             * \brief struct for ps keys and lens
             */
            struct PSKV {
                ps::SArray<ps::Key> keys;  // n keys
                ps::SArray<int> lens;  // the length of the i-th value
                int size;
            };

            struct ComprPSKV {
                PSKV push;
                PSKV pull;
            };

            /**
             * \brief cache all key partitions
             *
             * `ps_kv_` is used for pushes and pulls without gradient compression
             * `compr_ps_kv_` is used for gradient compression. It contains different
             * pskv for push and pull because sizes would be different in both cases.
             * Note: `ps_kv_[k]` for some key k may not be the same as `compr_ps_kv_[k].pull`
             * This is because sharding may cause slightly different divisions when size is
             * not perfectly divisible.
             */
            std::unordered_map<int, PSKV> ps_kv_;
            std::unordered_map<int, ComprPSKV> compr_ps_kv_;

            /**
             * \brief serialize access to ps_kv_ or push_ps_kv_/pull_ps_kv_ while encoding keys
             */
            std::mutex mu_;

            void InitImpl(const std::vector<int>& keys,
                          const std::vector<NDArray>& values) override {
              CheckUnique(keys);
              for (size_t i = 0; i < keys.size(); ++i) {
                comm_->Init(keys[i], values[i].storage_type(), values[i].shape(), values[i].dtype());
              }
              if (get_rank() == 0 && this->ps_worker_->get_customer()->customer_id() == 0) {
                Push_(keys, values, 0, false);
                // wait until the push is finished
                for (const int key : keys) {
                  comm_buf_[key].WaitToWrite();
                  compr_buf_[key].WaitToWrite();
                }
              } else {
                // do nothing
              }
              if (!ps::Postoffice::Get()->is_recovery()) {
                Barrier();
              }
            }

            void PushImpl(const std::vector<int>& keys,
                          const std::vector<NDArray>& values,
                          int priority) override {
              Push_(keys, values, priority, true);
            }

            void PullImpl(const std::vector<int>& keys,
                          const std::vector<NDArray*>& values,
                          int priority, bool ignore_sparse) override {
              CHECK(ignore_sparse) << "dist kvstore pull doesn't support ignore_sparse=False";
              std::vector<int> uniq_keys;
              std::vector<std::vector<NDArray*> > grouped_vals;
              GroupKVPairsPull(keys, values, &uniq_keys, &grouped_vals, true);
              for (size_t i = 0; i < uniq_keys.size(); ++i) {
                int key = uniq_keys[i];
                // use the same array for merging to guarantee that pull always happens
                // after the previous push on this key
                auto& recv_buf = comm_buf_[key];
                const auto storage_type = grouped_vals[i][0]->storage_type();
                CHECK_EQ(storage_type, kDefaultStorage)
                        << "Expected stype of value to be kDefaultStorage";
                if (recv_buf.is_none()) {
                  LOG(INFO)<<"recv is none";
                  // it may happen for the first time a no-rank-0 worker pull the weight.
                  recv_buf = NDArray(grouped_vals[i][0]->shape(), pinned_ctx_,
                                     true, grouped_vals[i][0]->dtype());
                  //}
                  auto pull_from_servers = [this, key, recv_buf](
                          RunContext rctx, Engine::CallbackOnComplete cb) {
                      // convert to ps keys
                      size_t size = recv_buf.shape().Size();
                      const int dtype = recv_buf.dtype();
                      const int num_bytes = mshadow::mshadow_sizeof(dtype);
                      LOG(INFO)<<"in pull, before encode, key is "<<key;//<<" and priority is "<<priority;
                      PSKV &pskv = (gradient_compression_->get_type() == CompressionType::kTwoBit) ?
                                   EncodeCompressedKey(key, size, false, num_bytes) :
                                   EncodeDefaultKey(key, size, num_bytes);
                      char *data = static_cast<char *> (recv_buf.data().dptr_);
                      // false means not to delete data when SArray is deleted
                      auto vals = new ps::SArray<char>(data, size * num_bytes, false);

                      // issue pull
                      RequestType mode = (gradient_compression_->get_type() != CompressionType::kNone) ?
                                         RequestType::kCompressedPushPull : RequestType::kDefaultPushPull;
                      const int cmd = GetCommandType(mode, dtype);

                      int len=0;
                      auto *counter = new std::atomic<int>(pskv.keys.size());
                      for (size_t i=0; i<pskv.keys.size(); i++) {
                        LOG(INFO)<<"key is "<<pskv.keys[i]<<" len is "<<pskv.lens[i];
                        auto vs = new ps::SArray<char>(std::move(vals->segment(len, len+pskv.lens[i])));
                        auto ls = new ps::SArray<int>(std::move(pskv.lens.segment(i, i+1)));
                        CHECK_NOTNULL(ps_worker_)->ZPull(
                                std::move(pskv.keys.segment(i, i+1)), vs, ls, cmd,
                                [vs, ls, vals, cb, counter]() {
                                    delete vs;
                                    delete ls;
                                    (*counter)--;
                                    if (counter->load() == 0) {
                                      delete vals;
                                      delete counter;
                                      cb();
                                    }
                                });
                        len+=pskv.lens[i];
                      }
                  };

                  CHECK_NOTNULL(Engine::Get())->PushAsync(
                          pull_from_servers,
                          pinned_ctx_,
                          {},
                          {recv_buf.var()},
                          FnProperty::kNormal,
                          priority,
                          "KVStoreDistDefaultStoragePull");
                }else{
                  LOG(INFO)<<"recv is not none";
                  CHECK_NOTNULL(Engine::Get())->PushAsync(
                          [] (RunContext rctx, Engine::CallbackOnComplete cb) { cb(); },
                          pinned_ctx_,
                          {},
                          {recv_buf.var()},
                          FnProperty::kNormal,
                          priority,
                          "KVStoreDistDefaultStoragePull");
                }
                comm_->Broadcast(key, recv_buf, grouped_vals[i], priority);
              }
            }

            /*
            void PullImpl(const std::vector<int>& keys,
                          const std::vector<NDArray*>& values,
                          int priority, bool ignore_sparse) override {
              CHECK(ignore_sparse) << "dist kvstore pull doesn't support ignore_sparse=False";
              std::vector<int> uniq_keys;
              std::vector<std::vector<NDArray*> > grouped_vals;
              GroupKVPairsPull(keys, values, &uniq_keys, &grouped_vals, true);

              for (size_t i = 0; i < uniq_keys.size(); ++i) {
                int key = uniq_keys[i];
                // use the same array for merging to guarantee that pull always happens
                // after the previous push on this key
                auto& recv_buf = comm_buf_[key];
                const auto storage_type = grouped_vals[i][0]->storage_type();
                CHECK_EQ(storage_type, kDefaultStorage)
                        << "Expected stype of value to be kDefaultStorage";
                if (recv_buf.is_none()) {
                  // it may happen for the first time a no-rank-0 worker pull the weight.
                  recv_buf = NDArray(grouped_vals[i][0]->shape(), pinned_ctx_,
                                     true, grouped_vals[i][0]->dtype());
                }
                auto pull_from_servers = [this, key, recv_buf](
                        RunContext rctx, Engine::CallbackOnComplete cb) {
                    // convert to ps keys
                    size_t size = recv_buf.shape().Size();
                    const int dtype = recv_buf.dtype();
                    const int num_bytes = mshadow::mshadow_sizeof(dtype);
                    PSKV& pskv = (gradient_compression_->get_type() == CompressionType::kNone) ?
                                 EncodeDefaultKey(key, size, num_bytes) :
                                 EncodeCompressedKey(key, size, false, num_bytes);
                    char* data = static_cast<char*> (recv_buf.data().dptr_);
                    // false means not to delete data when SArray is deleted
                    auto vals = new ps::SArray<char>(data, size * num_bytes, false);
                    // issue pull
                    RequestType mode = (gradient_compression_->get_type() != CompressionType::kNone) ?
                                       RequestType::kCompressedPushPull : RequestType::kDefaultPushPull;
                    const int cmd = GetCommandType(mode, dtype);
                    CHECK_NOTNULL(ps_worker_)->ZPull(
                            pskv.keys, vals, &pskv.lens, cmd, [vals, cb](){ delete vals; cb(); });
                };

                CHECK_NOTNULL(Engine::Get())->PushAsync(
                        pull_from_servers,
                        pinned_ctx_,
                        {},
                        {recv_buf.var()},
                        FnProperty::kNormal,
                        priority,
                        "KVStoreDistDefaultStoragePull");

                comm_->Broadcast(key, recv_buf, grouped_vals[i], priority);
              }
            }
*/
            void PullRowSparseImpl(const std::vector<int>& keys,
                                   const std::vector<std::pair<NDArray*, NDArray>>& val_rowids,
                                   int priority = 0) override {
              std::vector<int> uniq_keys;
              std::vector<std::vector<std::pair<NDArray*, NDArray>>> grouped_val_rowids;
              GroupKVPairsPullRsp(keys, val_rowids, &uniq_keys, &grouped_val_rowids, false);

              for (size_t i = 0; i < uniq_keys.size(); ++i) {
                int key = uniq_keys[i];
                // use the same array for merging to guarantee that pull always happens
                // after the previous push on this key
                auto& recv_buf = comm_buf_[key];
                auto& grouped_val_rowid = grouped_val_rowids[i];
                const auto storage_type = grouped_val_rowid[0].first->storage_type();
                CHECK_EQ(storage_type, kRowSparseStorage)
                        << "expected kRowSparseStorage, but got " << storage_type;
                if (recv_buf.is_none()) {
                  // it may happen for the first time a no-rank-0 worker pull the weight.
                  recv_buf = NDArray(storage_type, grouped_val_rowid[0].first->shape(),
                                     pinned_ctx_, true, grouped_val_rowid[0].first->dtype());
                }
                auto &target_val_rowids = grouped_val_rowids[i];
                const size_t num_vals = target_val_rowids.size();
                for (size_t i = 0; i < num_vals; i++) {
                  auto &row_id = target_val_rowids[i].second;
                  target_val_rowids[i].second = Unique(row_id, pinned_ctx_, 0);
                }
                CHECK_EQ(num_vals, 1) << "RowSparsePull with multiple values is not supported yet";
                NDArray& indices = target_val_rowids[0].second;
                PullRowSparse_(key, recv_buf, indices, priority);
                // The recv_buf contains values pulled from remote server with unique indices.
                // Directly broadcast w/o rowids if num_vals == 1
                auto get_val = [](const std::pair<NDArray*, NDArray>& p) { return p.first; };
                std::vector<NDArray*> grouped_val(grouped_val_rowid.size());
                std::transform(grouped_val_rowid.begin(), grouped_val_rowid.end(),
                               grouped_val.begin(), get_val);
                comm_->Broadcast(key, recv_buf, grouped_val, priority);
              }
            }

            void Push_(const std::vector<int>& keys,
                       const std::vector<NDArray>& values,
                       int priority,
                       bool do_merge) {
              // first aggregate the values over keys
              std::vector<int> uniq_keys;
              std::vector<std::vector<NDArray> > grouped_vals;
              GroupKVPairsPush(keys, values, &uniq_keys, &grouped_vals, false);
              LOG(INFO)<<"keys size is "<<keys.size()<<"keys[0] is "<<keys[0];
              bool init_flag=false;
              if(seg_update_flag==2){
                init_flag=true;
                LOG(INFO)<<"1 seg update flag is "<<seg_update_flag;
              }
              for (int i = 0; i < (int)uniq_keys.size(); ++i) {
                // merge over devices
                int key = uniq_keys[i];
                LOG(INFO)<<"in worker push_, key is "<<key<<"seg update flag is "<<seg_update_flag;
                if(seg_update_flag>=4){
                  seg_update_flag=1;
                  //i--;
                  LOG(INFO)<<"seg update flag is "<<seg_update_flag<<" and i is "<<i;
                }
                if (key==0 && seg_update_flag!=3){
                  if(task_cache){
                      seg_update_flag--;
                      task_cache=false;
                  }
                  seg_update_flag++;
                  if(seg_update_flag<3){
                    init_flag=true;
                  }else{
                    init_flag=false;
                  }
                  LOG(INFO)<<"2 seg update flag is "<<seg_update_flag;
                }
                if(init_flag){
                  LOG(INFO)<<"init flag is true, and key is  "<<key;
                  ps_kv_[key].keys.clear();
                  ps_kv_[key].lens.clear();
                  ps_kv_[key].size=0;

                }
                const auto& vals = grouped_vals[i];
                NDArray merged = do_merge ? comm_->Reduce(key, vals, priority) : vals[0];

                const auto storage_type = merged.storage_type();
                auto &comm_buf = comm_buf_[key];
                if (merged.ctx().dev_mask() == cpu::kDevMask) {
                  // Start of a push doesn't guarantee that the previous pushes are completed.
                  // This shouldn't affect training of networks though because training involves
                  // a sequence of push, pull, then push. This imposes ordering that the
                  // second push happens after the first pull, and the pull happens after first push.
                  comm_buf = merged;  // avoid memory copy
                } else {
                  if (comm_buf.is_none()) {
                    if (storage_type == kDefaultStorage) {
                      comm_buf = NDArray(merged.shape(), pinned_ctx_, true, merged.dtype());
                    } else {
                      comm_buf = NDArray(storage_type, merged.shape(), pinned_ctx_, true, merged.dtype());
                    }
                  }
                  CopyFromTo(merged, &comm_buf);
                }
                const int dtype = merged.dtype();
                const int num_bytes = mshadow::mshadow_sizeof(dtype);
                // push to servers
                if (storage_type == kDefaultStorage) {
                  if (gradient_compression_->get_type() == CompressionType::kNone) {
                    PSKV& pskv = EncodeDefaultKey(key, comm_buf.shape().Size(), num_bytes);
                    PushDefault(key, comm_buf, pskv, priority);
                  } else {
                    CHECK_EQ(dtype, mshadow::kFloat32) << "Gradient compression is only supported for "
                                                       << "float32 type of parameters";
                    // Note: gradient compression uses `do_merge` as proxy to
                    // detect whether the push is initialization of a key or not.
                    // is_active is false when push is initialization of key
                    bool is_active = do_merge;
                    PSKV &pskv = EncodeCompressedKey(key, comm_buf.shape().Size(), is_active, num_bytes);
                    // Returns push_pskv if active, else pull_pskv
                    // we want inactive gc to send uncompressed gradients,
                    // but sharded in the same way as later pushes would when gc becomes active
                    if (is_active) {
                      PushCompressed(key, comm_buf, pskv, priority);
                    } else {
                      PushDefault(key, comm_buf, pskv, priority);
                    }
                  }
                } else if (storage_type == kRowSparseStorage) {
                  CHECK(gradient_compression_->get_type() == CompressionType::kNone)
                          << "Gradient compression for row sparse storage type is not supported";
                  PushRowSparse(key, comm_buf, priority);
                } else {
                  LOG(FATAL) << "unknown storage type";
                }
              }
            }

            void PushCompressed(int key, const NDArray& comm_buf, const PSKV& pskv, int priority) {
              auto &small_buf = compr_buf_[key];
              auto &res_buf = residual_[key];
              const size_t original_size = comm_buf.shape().Size();
              const int dtype = comm_buf.dtype();

              // Init the small buffer and residual_ buffer for quantize
              if (small_buf.is_none()) {
                small_buf = NDArray(TShape{pskv.size}, comm_buf.ctx(), false, dtype);
                res_buf = NDArray(TShape{static_cast<int64_t>(original_size)}, comm_buf.ctx(), false, dtype);
                res_buf = 0;
              }
              gradient_compression_->Quantize(comm_buf, &small_buf, &res_buf, priority);
              auto push_to_servers =
                      [this, key, dtype, pskv, small_buf](RunContext rctx, Engine::CallbackOnComplete cb) {
                          size_t size = small_buf.shape().Size() * mshadow::mshadow_sizeof(dtype);
                          char* data = static_cast<char *> (small_buf.data().dptr_);
                          // do push. false means no delete
                          ps::SArray<char> vals(data, size, false);
                          int cmd = GetCommandType(RequestType::kCompressedPushPull, dtype);
                          CHECK_NOTNULL(ps_worker_)->ZPush(pskv.keys, vals, pskv.lens, cmd, [cb]() { cb(); });
                      };
              // acquire locks on both comm_buf and small_buf so that
              // pull (which uses comm_buf) for the same key waits till push finishes
              Engine::Get()->PushAsync(
                      push_to_servers,
                      pinned_ctx_,
                      {small_buf.var(), comm_buf.var()},
                      {},
                      FnProperty::kNormal,
                      priority,
                      "KVStoreDistCompressedPush");
            }

            void PushDefault(int key, const NDArray &send_buf, const PSKV& pskv, int priority) {
              auto push_to_servers =
                      [this, key, pskv, send_buf](RunContext rctx, Engine::CallbackOnComplete cb) {
                          // LOG(INFO) << "send_buf.dtype() " << send_buf.dtype();
                          const int dtype = send_buf.dtype();
                          // convert to ps keys
                          // LOG(INFO) << "send_buf.shape().Size() " << send_buf.shape().Size();
                          // LOG(INFO) << "mshadow::mshadow_sizeof(dtype) " << mshadow::mshadow_sizeof(dtype);
                          const size_t size = send_buf.shape().Size() * mshadow::mshadow_sizeof(dtype);
                          char* data = static_cast<char *>(send_buf.data().dptr_);
                          // do push. false means no delete
                          ps::SArray<char> vals(data, size, false);
                          int cmd = GetCommandType(RequestType::kDefaultPushPull, dtype);
                          // LOG(INFO) << "cmd = GetCommandType " << cmd;
                          auto *counter = new std::atomic<int>(pskv.keys.size());
                          int len=0;
                          for (size_t i=0; i<pskv.keys.size(); i++) {
                            LOG(INFO)<<"key is "<<key<<"key son is "<<pskv.keys[i]<<" len is "<<pskv.lens[i];//<<" priority is "<<priority;
                            CHECK_NOTNULL(ps_worker_)->ZPush(
                                    std::move(pskv.keys.segment(i, i+1)),
                                    std::move(vals.segment(len, len+pskv.lens[i])),
                                    std::move(pskv.lens.segment(i, i+1)),
                                    cmd,
                                    [cb, counter]() {
                                        //engine::SetOprEnd(opr_stat);
                                        (*counter)--;
                                        if (counter->load() == 0) {
                                          delete counter;
                                          cb();
                                        }
                                    });
                            len+=pskv.lens[i];
                          }
                      };
              Engine::Get()->PushAsync(
                      push_to_servers,
                      pinned_ctx_,
                      {send_buf.var()},
                      {},
                      FnProperty::kNormal,
                      priority,
                      "KVStoreDistDefaultPush");
            }
            /*
            void PushDefault(int key, const NDArray &send_buf, const PSKV& pskv, int priority) {
              auto push_to_servers =
                      [this, key, pskv, send_buf](RunContext rctx, Engine::CallbackOnComplete cb) {
                          const int dtype = send_buf.dtype();
                          // convert to ps keys
                          const size_t size = send_buf.shape().Size() * mshadow::mshadow_sizeof(dtype);
                          char* data = static_cast<char *>(send_buf.data().dptr_);
                          // do push. false means no delete
                          ps::SArray<char> vals(data, size, false);
                          int cmd = GetCommandType(RequestType::kDefaultPushPull, dtype);
                          CHECK_NOTNULL(ps_worker_)->ZPush(
                                  pskv.keys, vals, pskv.lens,
                                  cmd, [cb]() { cb(); });
                      };
              Engine::Get()->PushAsync(
                      push_to_servers,
                      pinned_ctx_,
                      {send_buf.var()},
                      {},
                      FnProperty::kNormal,
                      priority,
                      "KVStoreDistDefaultPush");
            }
*/
            // push row sparse gradient
            void PushRowSparse(int key, const NDArray &send_buf, int priority) {
              using namespace rowsparse;
              auto push_to_servers = [this, key, send_buf]
                      (RunContext rctx, Engine::CallbackOnComplete cb) {
                  char* data = static_cast<char *>(send_buf.data().dptr_);
                  const int64_t num_rows = send_buf.aux_shape(kIdx)[0];
                  const auto offsets = send_buf.aux_data(kIdx).dptr<int64_t>();
                  const auto unit_len = send_buf.shape().ProdShape(1, send_buf.shape().ndim());
                  const int num_bytes = mshadow::mshadow_sizeof(send_buf.dtype());
                  const int64_t size = num_rows * unit_len;
                  // convert to ps keys in row sparse format
                  PSKV& pskv = EncodeRowSparseKey(key, size, num_rows, offsets,
                                                  unit_len, send_buf.shape()[0], num_bytes);
                  if (this->log_verbose_) {
                    LOG(INFO) << "worker " << get_rank() << " push lens: " << pskv.lens << " keys: "
                              << pskv.keys << " size: " << size;
                  }
                  ps::SArray<char> vals(data, size * num_bytes, false);
                  const int cmd = GetCommandType(RequestType::kRowSparsePushPull, send_buf.dtype());
                  CHECK_NOTNULL(ps_worker_)->ZPush(pskv.keys, vals, pskv.lens, cmd, [cb]() { cb(); });
              };
              Engine::Get()->PushAsync(
                      push_to_servers,
                      pinned_ctx_,
                      {send_buf.var()},
                      {},
                      FnProperty::kNormal,
                      priority,
                      "KVStoreDistRowSparsePush");
            }


            // pull row sparse weight into `recv_buf` based on indices given by `indices`
            void PullRowSparse_(const int key, const NDArray& recv_buf,
                                const NDArray& indices, int priority) {
              using namespace rowsparse;
              auto pull_from_servers = [this, key, recv_buf, indices]
                      (RunContext rctx, Engine::CallbackOnComplete cb) {
                  // allocate memory for the buffer
                  CHECK_EQ(indices.dtype(), mshadow::kInt64);
                  const TBlob idx_data = indices.data();
                  const size_t num_rows = idx_data.shape_.Size();
                  recv_buf.CheckAndAlloc({mshadow::Shape1(num_rows)});
                  const int dtype = recv_buf.dtype();
                  char* data = static_cast<char *>(recv_buf.data().dptr_);
                  const auto offsets = idx_data.dptr<int64_t>();
                  const auto unit_len = recv_buf.shape().ProdShape(1, recv_buf.shape().ndim());
                  const int64_t size = num_rows * unit_len;
                  const int num_bytes = mshadow::mshadow_sizeof(dtype);
                  // convert to ps keys in row sparse format
                  PSKV& pskv = EncodeRowSparseKey(key, size, num_rows, offsets,
                                                  unit_len, recv_buf.shape()[0],
                                                  num_bytes);
                  if (this->log_verbose_) {
                    LOG(INFO) << "worker " << get_rank() << " pull lens: " << pskv.lens << " keys: "
                              << pskv.keys << " size: " << size;
                  }
                  auto vals = new ps::SArray<char>(data, size * num_bytes, false);
                  const int cmd = GetCommandType(RequestType::kRowSparsePushPull, recv_buf.dtype());
                  // copy indices to recv_buf. this needs to be done before ZPull
                  // because after pull is done, the callback function returns and locks are released.
                  // at this point, later functions may access the indices variable while copy happens
                  mshadow::Copy(recv_buf.aux_data(kIdx).FlatTo1D<cpu, int64_t>(),
                                idx_data.FlatTo1D<cpu, int64_t>());
                  CHECK_NOTNULL(ps_worker_)->ZPull(pskv.keys, vals, &pskv.lens,
                                                   cmd,
                                                   [vals, cb]() { delete vals; cb(); });
              };
              CHECK_NOTNULL(Engine::Get())->PushAsync(
                      pull_from_servers,
                      pinned_ctx_,
                      {indices.var()},
                      {recv_buf.var()},
                      FnProperty::kNormal,
                      priority,
                      "KVStoreDistRowSparsePull");
            }

            /**
             * \brief check if the keys are all unique
             */
            void CheckUnique(const std::vector<int>& keys) {
              auto keys_copy = keys;
              auto last = std::unique(keys_copy.begin(), keys_copy.end());
              CHECK_EQ(static_cast<size_t>(std::distance(keys_copy.begin(), last)),
                       static_cast<size_t>(keys.size()));
            }

            /**
             * \brief convert to pskv for parameter server
             * \param key
             * \param num_arr_elems number of elements in the value for key
             * \param num_bytes size of each element in number of bytes
             * \return PSKV used for both push and pull
             */
            void TaskDivisionUpdate(std::vector<float> perf){
                if(seg_update_flag!=3){
                    if(seg_update_flag==4){
                        server_perf=perf;
                    }else if(seg_update_flag==2){
                        server_perf=perf;
                        task_cache=true;
                    }
                    return;
                }
                server_perf=perf;
                seg_update_flag++;
                LOG(INFO)<<"it's worker task division update, and perf is : ";
                for(auto it:server_perf)std::cout<<it<<" ";std::cout<<std::endl;
            }
            inline PSKV& EncodeDefaultKey(const int key, const size_t num_arr_elems,
                                          const int num_bytes) {
              mu_.lock();
              if(num_para_flag<2){
                  if(key==0){
                      num_para_flag++;
                      if(num_para_count.size()==0){
                          for(size_t i=0;i<ps::Postoffice::Get()->GetServerKeyRanges().size();i++){
                              num_para_count.push_back(0);
                          }
                      }
                  }
                  if(num_para_flag==1){
                      LOG(INFO)<<"key is "<<key<<" para num is "<<num_arr_elems<<"tatal num para is "<<num_para;
                      num_para+=num_arr_elems;
                  }
              }
              if(num_para_flag==2){
                  LOG(INFO)<<"para num is "<<num_para;
                  num_para_flag++;
              }
              PSKV& pskv = ps_kv_[key];
              mu_.unlock();
              size_t pskv_size = num_arr_elems * num_bytes;
              if (!pskv.keys.empty()) {
                CHECK_EQ(static_cast<size_t>(pskv.size), pskv_size)
                        << "The value size cannot be changed " << pskv_size << ". Key is " << key;
              } else {
                auto krs = ps::Postoffice::Get()->GetServerKeyRanges();
                const int num_servers = krs.size();
                CHECK_GT(num_servers, 0);
                // a simple heuristic for load balance
                if(enable_original==1){
                    // a simple heuristic for load balance
                    if (num_arr_elems < bigarray_bound_) {
                        // send it to a single random picked server
                        int server = (key * 9973) % num_servers;
                        ps::Key ps_key = krs[server].begin() + key;
                        CHECK_LT(ps_key, krs[server].end());
                        pskv.keys.push_back(ps_key);
                        const int total_bytes = num_arr_elems * num_bytes;
                        pskv.lens.push_back(total_bytes);
                        pskv.size = total_bytes;
                    } else {
                        // parition it to all servers
                        pskv.size = 0;
                        for (int i = 0; i < num_servers; ++i) {
                            size_t part_size =
                                    static_cast<size_t>(round(static_cast<double>(num_arr_elems)/num_servers*(i+1))) -
                                    static_cast<size_t>(round(static_cast<double>(num_arr_elems)/num_servers*i));
                            ps::Key ps_key = krs[i].begin() + key;
                            CHECK_LT(ps_key, krs[i].end());
                            pskv.keys.push_back(ps_key);
                            const int total_bytes = part_size * num_bytes;
                            pskv.lens.push_back(total_bytes);
                            pskv.size += total_bytes;
                        }
                    }
                }else{
                    LOG(INFO)<<"in bigger than big array";
                    // parition it to all servers
                    if(key==0) {
                        if(seg_update_flag!=2){
                            server_log=0;
                            for(int i=0;i<num_servers;i++){
                                key_temp[i]=krs[i].begin();
                                num_para_count[i]=0;
                            }
                        }
                    }
                    pskv.size = 0;
                    size_t s = num_arr_elems;
                    while(true) {
                        ps::Key ps_key;
                        if(seg_update_flag==2){
                            if(key==0){
                                int num_seg=0;
                                float perf_sum=0;
                                for(int i=0;i<num_servers;i++){
                                    num_seg+=( key_temp[i]-krs[i].begin());
                                    key_temp[i]=krs[i].begin();
                                    perf_sum+=server_perf[i];
                                }
                                for(int i=0;i<num_servers;i++){
                                    server_perf[i]=(float)ceil(server_perf[i]/perf_sum*num_para);
                                }
                                //server_perf[num_servers-1]=(float)server_log-1;
                                LOG(INFO)<<"num seg is "<<num_seg;
                                for(auto it:server_perf)std::cout<<it<<" ";std::cout<<std::endl;
                                server_log=0;
                                for(int i=0;i<num_servers;i++)num_para_count[i]=0;
                            }
                            while(num_para_count[server_log%num_servers]>=server_perf[server_log%num_servers]){
                                server_log++;
                                LOG(INFO)<<"server_log is "<<server_log;
                            }
                            ps_key = key_temp[server_log%num_servers]++;
                            CHECK_LT(ps_key, krs[server_log%num_servers].end());
                            LOG(INFO)<<"server is "<<server_log%num_servers;

                        }else{
                            ps_key = key_temp[server_log%num_servers]++;
                            CHECK_LT(ps_key, krs[server_log%num_servers].end());
                            LOG(INFO)<<"server is "<<server_log%num_servers;
                        }
                        pskv.keys.push_back(ps_key);
                        LOG(INFO)<<"server_log is "<<server_log;
//                        server_log++;
                        LOG(INFO)<<"from key "<<key<<" ,key son is "<<ps_key;
                        if (s > bigarray_bound_) {
                            pskv.lens.push_back(bigarray_bound_*num_bytes);
                            pskv.size += (bigarray_bound_*num_bytes);
                            LOG(INFO)<<"len is "<<bigarray_bound_*num_bytes;
                            num_para_count[server_log%num_servers]+=bigarray_bound_;
                            server_log++;
                        } else {
                            pskv.lens.push_back(s*num_bytes);
                            pskv.size += (s*num_bytes);
                            LOG(INFO)<<"len is "<<s*num_bytes;
                            num_para_count[server_log%num_servers]+=s;
                            server_log++;
                            break;
                        }
                        s -= bigarray_bound_;
                    }
                }
                //}
                CHECK_EQ(static_cast<size_t>(pskv.size), pskv_size);
              }
              return pskv;
            }
 /*
            inline PSKV& EncodeDefaultKey(const int key, const size_t num_arr_elems,
                                          const int num_bytes) {
              mu_.lock();
              PSKV& pskv = ps_kv_[key];
              mu_.unlock();
              size_t pskv_size = num_arr_elems * num_bytes;
              if (!pskv.keys.empty()) {
                CHECK_EQ(static_cast<size_t>(pskv.size), pskv_size)
                        << "The value size cannot be changed " << pskv_size << ". Key is " << key;
              } else {
                auto krs = ps::Postoffice::Get()->GetServerKeyRanges();
                const int num_servers = krs.size();
                CHECK_GT(num_servers, 0);

                // a simple heuristic for load balance
                if (num_arr_elems < bigarray_bound_) {
                  // send it to a single random picked server
                  int server = (key * 9973) % num_servers;
                  ps::Key ps_key = krs[server].begin() + key;
                  CHECK_LT(ps_key, krs[server].end());
                  pskv.keys.push_back(ps_key);
                  const int total_bytes = num_arr_elems * num_bytes;
                  pskv.lens.push_back(total_bytes);
                  pskv.size = total_bytes;
                } else {
                  // parition it to all servers
                  pskv.size = 0;
                  for (int i = 0; i < num_servers; ++i) {
                    size_t part_size =
                            static_cast<size_t>(round(static_cast<double>(num_arr_elems)/num_servers*(i+1))) -
                            static_cast<size_t>(round(static_cast<double>(num_arr_elems)/num_servers*i));
                    ps::Key ps_key = krs[i].begin() + key;
                    CHECK_LT(ps_key, krs[i].end());
                    pskv.keys.push_back(ps_key);
                    const int total_bytes = part_size * num_bytes;
                    pskv.lens.push_back(total_bytes);
                    pskv.size += total_bytes;
                  }
                }
                CHECK_EQ(static_cast<size_t>(pskv.size), pskv_size);
              }
              return pskv;
            }
*/
            /**
             * \brief Convert to PSKV for pushes and pulls when gradient compression is used.
             * Divides original array into equal parts for each server.
             * Populates both push and pull pskv on first call.
             * \param key
             * \param num_arr_elems number of elements in the value for key
             * \param is_push whether this is push or pull
             * \param num_bytes size of each element in number of bytes
             * \return PSKV used for both push and pull
             */
            inline PSKV& EncodeCompressedKey(const int key, const size_t original_num_elem,
                                             const bool is_push, const int num_bytes) {
              auto krs = ps::Postoffice::Get()->GetServerKeyRanges();
              const int num_servers = krs.size();
              CHECK_GT(num_servers, 0);

              // represents size of data to be sent
              size_t compr_num_elem = gradient_compression_->GetCompressedSize(original_num_elem);
              mu_.lock();
              PSKV& pskv = (is_push) ? compr_ps_kv_[key].push : compr_ps_kv_[key].pull;
              mu_.unlock();

              if (!pskv.keys.empty()) {
                const size_t num_elem = (is_push) ? compr_num_elem : original_num_elem;
                CHECK_EQ(static_cast<size_t >(pskv.size), num_elem * num_bytes)
                        << "The value size can't be changed. For key " << key;
              } else {
                // populate both pull and push pskvs
                // push pskv has sizes corresponding to compressed data
                // pull pskv has decompressed sizes for parts in push_pskv
                mu_.lock();
                PSKV& pull_pskv = compr_ps_kv_[key].pull;
                PSKV& push_pskv = compr_ps_kv_[key].push;
                mu_.unlock();

                if (original_num_elem < bigarray_bound_) {
                  // a simple heuristic for load balancing
                  // send it to a single random picked server
                  const int server = (key * 9973) % num_servers;
                  ps::Key ps_key = krs[server].begin() + key;
                  CHECK_LT(ps_key, krs[server].end());
                  // meta info
                  push_pskv.keys.push_back(krs[server].begin() + original_num_elem);
                  push_pskv.lens.push_back(0);
                  // data
                  push_pskv.keys.push_back(ps_key);
                  pull_pskv.keys.push_back(ps_key);
                  const int compr_size = compr_num_elem * num_bytes;
                  const int original_size = original_num_elem * num_bytes;
                  push_pskv.lens.push_back(compr_size);
                  pull_pskv.lens.push_back(original_size);
                  push_pskv.size = compr_size;
                  pull_pskv.size = original_size;
                } else {
                  // partition it to all servers
                  push_pskv.size = 0;
                  pull_pskv.size = 0;

                  for (int i = 0; i < num_servers; ++i) {
                    size_t part_compr, part_orig;
                    if (i == num_servers-1) {
                      part_compr = compr_num_elem - push_pskv.size;
                      part_orig = original_num_elem - pull_pskv.size;
                    } else {
                      part_compr =
                              static_cast<size_t> (round(static_cast<double>(compr_num_elem)/num_servers*(i+1))) -
                              static_cast<size_t> (round(static_cast<double>(compr_num_elem)/num_servers*(i)));
                      part_orig = part_compr * gradient_compression_->GetCompressionFactor();
                    }

                    // meta info
                    ps::Key ps_key_dummy = krs[i].begin() + part_orig;
                    CHECK_LT(ps_key_dummy, krs[i].end());
                    push_pskv.keys.push_back(ps_key_dummy);
                    push_pskv.lens.push_back(0);

                    // data
                    ps::Key ps_key = krs[i].begin() + key;
                    CHECK_LT(ps_key, krs[i].end());
                    push_pskv.keys.push_back(ps_key);
                    pull_pskv.keys.push_back(ps_key);
                    push_pskv.lens.push_back(part_compr * num_bytes);
                    pull_pskv.lens.push_back(part_orig * num_bytes);
                    // num elements need to be inserted below so that for last server,
                    // there is no round off error
                    push_pskv.size += part_compr;
                    pull_pskv.size += part_orig;
                  }
                  CHECK_EQ(static_cast<size_t>(push_pskv.size), compr_num_elem);
                  CHECK_EQ(static_cast<size_t>(pull_pskv.size), original_num_elem);
                  push_pskv.size *= num_bytes;
                  pull_pskv.size *= num_bytes;
                  CHECK_EQ(push_pskv.lens.size(), num_servers * 2);
                }
              }
              return pskv;
            }

            // Note: this encoding method for row sparse keys doesn't allow cross-layer batching
            inline PSKV& EncodeRowSparseKey(const int key, const int64_t num_elem, const int64_t num_rows,
                                            const int64_t *offsets, const size_t unit_len,
                                            const int64_t total_num_rows, const int num_bytes) {
              using namespace common;
              mu_.lock();
              PSKV& pskv = ps_kv_[key];
              mu_.unlock();
              pskv.keys.clear();
              pskv.lens.clear();
              // TODO(haibin) cache this information
              auto krs = ps::Postoffice::Get()->GetServerKeyRanges();
              const int num_servers = krs.size();
              CHECK_GT(num_servers, 0);

              if (total_num_rows * unit_len >= bigarray_bound_) {
                pskv.size = 0;
                int64_t start_row = 0;
                // parition it to all servers
                for (int i = 0; i < num_servers; ++i) {
                  ps::Key master_key = krs[i].begin() + key;
                  pskv.keys.push_back(master_key);
                  pskv.lens.push_back(0);
                  if (offsets && num_elem > 0) {
                    // calculate partition ranges
                    int64_t part_num_rows =
                            llround(static_cast<double>(total_num_rows) / num_servers * (i + 1)) -
                            llround(static_cast<double>(total_num_rows) / num_servers * i);
                    auto end_row = start_row + part_num_rows;
                    // search for offsets in [start_row, end_row)
                    auto lb = std::lower_bound(offsets, offsets + num_rows, start_row);
                    auto ub = std::upper_bound(offsets, offsets + num_rows, end_row - 1);
                    for (auto offset = lb; offset < ub; offset++) {
                      ps::Key ps_key = krs[i].begin() + key + (*offset - start_row);
                      CHECK_LT(ps_key, krs[i].end());
                      pskv.keys.push_back(ps_key);
                      const int part_size = unit_len * num_bytes;
                      pskv.lens.push_back(part_size);
                      pskv.size += (part_size);
                    }
                    start_row = end_row;
                  }
                }
                CHECK_EQ(static_cast<size_t>(pskv.size), num_elem * num_bytes);
              } else {
                // send it to a single random picked server
                const int server = (key * 9973) % num_servers;
                ps::Key master_key = krs[server].begin() + key;
                pskv.keys.push_back(master_key);
                pskv.lens.push_back(0);
                for (int64_t i = 0; i < num_rows; i++) {
                  ps::Key ps_key = krs[server].begin() + key + offsets[i];
                  CHECK_LT(ps_key, krs[server].end());
                  pskv.keys.push_back(ps_key);
                  pskv.lens.push_back(unit_len * num_bytes);
                }
                pskv.size = num_elem * num_bytes;
              }
              return pskv;
            }

            /**
             * \brief for worker to push and pull data
             */
            ps::KVWorker<char>* ps_worker_;
            /**
             * \brief the server handle
             */
            KVStoreDistServer* server_;
            /**
             * \brief threshold for partition
             */
            size_t bigarray_bound_;
            /**
             * \brief buffer for non-compressed data.
             * When gradient compression is active, this is used
             * for the data in pull and for original data in push
             */
            std::unordered_map<int, NDArray> comm_buf_;
            /**
             * \brief buffer for compressed data
             * Used when gradient compression is active and action
             * is push
             */
            std::unordered_map<int, NDArray> compr_buf_;
            /**
             * \brief residual buffer to accumulate quantization error
             * during gradient compression
             */
            std::unordered_map<int, NDArray> residual_;
            bool log_verbose_;
            std::unordered_map<int, ps::Key> key_temp;
            std::vector<float> server_perf;
            size_t seg_update_flag=3;
            int server_log=0;
            //std::vector<size_t> server_task_assign;
            int num_seg=0;
            float perf_sum=0;
            size_t enable_original;
            bool task_cache=false;
            size_t num_para=0;
            int num_para_flag=0;
            std::vector<size_t> num_para_count;
        };

    }  // namespace kvstore
}  // namespace mxnet


#endif  // MXNET_KVSTORE_KVSTORE_DIST_H_
