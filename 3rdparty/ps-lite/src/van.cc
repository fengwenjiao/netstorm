/**
*  Copyright (c) 2015 by Contributors
*/
#include "ps/internal/van.h"
#include <thread>
#include <chrono>
#include <mutex>
#include <ctime>
#include <iostream>
#include <condition_variable>
#include "ps/base.h"
#include "ps/sarray.h"
#include "ps/internal/postoffice.h"
#include "ps/internal/customer.h"
#include "./network_utils.h"
#include "meta.pb.h"
#include "./zmq_van.h"
#include "./resender.h"
#include "ps/sarray.h"
#include "ps/internal/threadsafe_queue.h"
namespace ps {

// interval in second between to heartbeast signals. 0 means no heartbeat.
// don't send heartbeast in default. because if the scheduler received a
// heartbeart signal from a node before connected to that node, then it could be
// problem.
static const int kDefaultHeartbeatInterval = 0;

Van* Van::Create(const std::string &type) {
    if (type == "zmq") {
        return new ZMQVan();
    } else {
        LOG(FATAL) << "unsupported van type: " << type;
        return nullptr;
    }
}

void Van::ProcessTerminateCommand() {
    PS_VLOG(1) << my_node().ShortDebugString() << " is stopped";
    ready_ = false;
}

void Van::ProcessAddNodeCommandAtScheduler(
        Message *msg, Meta *nodes, Meta *recovery_nodes) {
    recovery_nodes->control.cmd = Control::ADD_NODE;
    time_t t = time(NULL);
    size_t num_nodes = Postoffice::Get()->num_servers() + Postoffice::Get()->num_workers();
    if (nodes->control.node.size() == num_nodes) {
        // sort the nodes according their ip and port,
        std::sort(nodes->control.node.begin(), nodes->control.node.end(),
                    [](const Node& a, const Node& b) {
                        return (a.hostname.compare(b.hostname) | (a.port < b.port)) > 0;
                    });
        // assign node rank
        for (auto &node : nodes->control.node) {
            std::string node_host_ip = node.hostname + ":" + std::to_string(node.port);
            if (connected_nodes_.find(node_host_ip) == connected_nodes_.end()) {
                CHECK_EQ(node.id, Node::kEmpty);
                int id = node.role == Node::SERVER ?
                            Postoffice::ServerRankToID(num_servers_) :
                            Postoffice::WorkerRankToID(num_workers_);
                if (node.role == Node::WORKER || node.role == Node::SERVER)id = node.id_backup;
                PS_VLOG(1) << "assign rank=" << id << " to node " << node.DebugString();
                node.id = id;
                Connect_Eth(my_node_, node);
                Postoffice::Get()->UpdateHeartbeat(node.id, t);
                connected_nodes_[node_host_ip] = id;
            } else {
                int id = node.role == Node::SERVER ?
                            Postoffice::ServerRankToID(num_servers_) :
                            Postoffice::WorkerRankToID(num_workers_);
                shared_node_mapping_[id] = connected_nodes_[node_host_ip];
                node.id = connected_nodes_[node_host_ip];
            }
            if (node.role == Node::SERVER) num_servers_++;
            if (node.role == Node::WORKER) num_workers_++;
        }
        nodes->control.node.push_back(my_node_);
        nodes->control.cmd = Control::ADD_NODE;
        Message back;
        back.meta = *nodes;
        for (int r : Postoffice::Get()->GetNodeIDs(kWorkerGroup + kServerGroup)) {
            int recver_id = r;
            if (shared_node_mapping_.find(r) == shared_node_mapping_.end()) {
                back.meta.recver = recver_id;
                back.meta.timestamp = timestamp_++;
                back.meta.sender = my_node_.id;
                back.meta.sender_ = my_node_.id;
                Send(back);
            }
            PS_VLOG(2) << "the scheduler is connected to "<<recver_id;
        }
        PS_VLOG(2) << "the scheduler is connected to "
                    << num_workers_ << " workers and " << num_servers_ << " servers";
        ready_ = true;
    } else if (!recovery_nodes->control.node.empty()) {
        auto dead_nodes = Postoffice::Get()->GetDeadNodes(heartbeat_timeout_);
        std::unordered_set<int> dead_set(dead_nodes.begin(), dead_nodes.end());
        // send back the recovery node
        CHECK_EQ(recovery_nodes->control.node.size(), 1);
        Connect(recovery_nodes->control.node[0]);
        Postoffice::Get()->UpdateHeartbeat(recovery_nodes->control.node[0].id, t);
        Message back;
        for (int r : Postoffice::Get()->GetNodeIDs(kWorkerGroup + kServerGroup)) {
            if (r != recovery_nodes->control.node[0].id
                && dead_set.find(r) != dead_set.end()) {
                // do not try to send anything to dead node
                continue;
            }
            // only send recovery_node to nodes already exist
            // but send all nodes to the recovery_node
            back.meta = (r == recovery_nodes->control.node[0].id) ? *nodes : *recovery_nodes;
            back.meta.recver = r;
            back.meta.timestamp = timestamp_++;
            Send(back);
            PS_VLOG(2) << "ADD_NODE back to node " << r;
        }
    }
}

void Van::UpdateLocalID(Message *msg, std::unordered_set<int> *deadnodes_set,
                        Meta* nodes, Meta* recovery_nodes) {
    auto& ctrl = msg->meta.control;
    std::size_t num_nodes = Postoffice::Get()->num_servers() + Postoffice::Get()->num_workers();
    // assign an id
    if (msg->meta.sender_ == Meta::kEmpty) {
        PS_VLOG(2) << "COME IN";
        CHECK(is_scheduler_);
        CHECK_EQ(ctrl.node.size(), 1);
        if (nodes->control.node.size() < num_nodes) {
            PS_VLOG(2) << " a new node push bach in secheuler";
            nodes->control.node.push_back(ctrl.node[0]);
        } else {
            // some node dies and restarts
            CHECK(ready_.load());
            for (size_t i = 0; i < nodes->control.node.size() - 1; ++i) {
                const auto& node = nodes->control.node[i];
                if (deadnodes_set->find(node.id) != deadnodes_set->end() &&
                    node.role == ctrl.node[0].role) {
                    auto& recovery_node = ctrl.node[0];
                    // assign previous node id
                    recovery_node.id = node.id;
                    recovery_node.is_recovery = true;
                    PS_VLOG(1) << "replace dead node " << node.DebugString()
                                << " by node " << recovery_node.DebugString();
                    nodes->control.node[i] = recovery_node;
                    recovery_nodes->control.node.push_back(recovery_node);
                    break;
                }
            }
        }
    }

    // update my id
    for (size_t i = 0; i < ctrl.node.size(); ++i) {
        const auto& node = ctrl.node[i];
        if (my_node_.hostname == node.hostname && my_node_.port == node.port) {
            if (getenv("DMLC_RANK") == nullptr || my_node_.id == Meta::kEmpty) {
                my_node_ = node;
                std::string rank = std::to_string(Postoffice::IDtoRank(node.id));
#ifdef _MSC_VER
                _putenv_s("DMLC_RANK", rank.c_str());
#else
                setenv("DMLC_RANK", rank.c_str(), true);
#endif
            }
        }
    }
}

void Van::ProcessHearbeat(Message *msg) {
    auto& ctrl = msg->meta.control;
    time_t t = time(NULL);
    for (auto &node : ctrl.node) {
        Postoffice::Get()->UpdateHeartbeat(node.id, t);
        if (is_scheduler_) {
            Message heartbeat_ack;
            heartbeat_ack.meta.recver = node.id;
            heartbeat_ack.meta.control.cmd = Control::HEARTBEAT;
            heartbeat_ack.meta.control.node.push_back(my_node_);
            heartbeat_ack.meta.timestamp = timestamp_++;
            // send back heartbeat
            Send(heartbeat_ack);
        }
    }
}

void Van::ProcessBarrierCommand(Message *msg) {
    auto& ctrl = msg->meta.control;
    if (msg->meta.request) {
        if (barrier_count_.empty()) {
            barrier_count_.resize(8, 0);
        }
        int group = ctrl.barrier_group;
        ++barrier_count_[group];
        PS_VLOG(1) << "Barrier count for " << group << " : " << barrier_count_[group];
        if (barrier_count_[group] ==
            static_cast<int>(Postoffice::Get()->GetNodeIDs(group).size())) {
            barrier_count_[group] = 0;
            Message res;
            res.meta.request = false;
            res.meta.sender_ = my_node_.id;
            res.meta.app_id = msg->meta.app_id;
            res.meta.customer_id = msg->meta.customer_id;
            res.meta.control.cmd = Control::BARRIER;
            for (int r : Postoffice::Get()->GetNodeIDs(group)) {
                int recver_id = r;
                if (shared_node_mapping_.find(r) == shared_node_mapping_.end()) {
                    res.meta.recver = recver_id;
                    res.meta.dst = recver_id;
                    res.meta.timestamp = timestamp_++;
                    CHECK_GT(Send(res), 0);
                }
            }
        }
    } else {
        Postoffice::Get()->Manage(*msg);
    }
}

void Van::ProcessDataMsg(Message *msg) {
    if (msg->meta.control.cmd == Control::REPLY1) {
        msg->meta.dst = Meta::kEmpty;
        msg->meta.control.cmd = Control::EMPTY;
        size_t num_nodes = msg->meta.control.node.size();
        for (size_t i = 0; i < num_nodes; i++)msg->meta.control.node.pop_back();
        if(msg->meta.tree.size()){
            if(my_node_.id==9)msg->meta.timestamp++;
        }
        if(Postoffice::Get()->is_worker() && msg->data.size()>1){
            msg->meta.timestamp=key_timestamp[msg->meta.key_data];
        }
        PS_VLOG(2) << "a data msg received: " << msg->DebugString();
    }

    // data msg
    CHECK_NE(msg->meta.sender, Meta::kEmpty);
    CHECK_NE(msg->meta.recver, Meta::kEmpty);
    CHECK_NE(msg->meta.app_id, Meta::kEmpty);
    int app_id = msg->meta.app_id;
    int customer_id = Postoffice::Get()->is_worker() ? msg->meta.customer_id : app_id;
    auto* obj = Postoffice::Get()->GetCustomer(app_id, customer_id, 5);
    CHECK(obj) << "timeout (5 sec) to wait App " << app_id << " customer " << customer_id \
<< " ready at " << my_node_.role;
    obj->Accept(*msg);
}

void Van::ProcessAddNodeCommand(Message *msg, Meta *nodes, Meta *recovery_nodes) {
    auto dead_nodes = Postoffice::Get()->GetDeadNodes(heartbeat_timeout_);
    std::unordered_set<int> dead_set(dead_nodes.begin(), dead_nodes.end());
    UpdateLocalID(msg, &dead_set, nodes, recovery_nodes);

    for (std::size_t i = 0; i < my_node_group.size(); i++)my_node_group[i].id = my_node_.id;

    if (is_scheduler_) {
        ProcessAddNodeCommandAtScheduler(msg, nodes, recovery_nodes);
        PS_VLOG(2) << "ADD NODE OK";
    } else {
        //int container_id;
        const char *pstr = Environment::Get()->find("CONTAINER_ID");
        if (pstr) my_container_id = atoi(pstr);
        PS_VLOG(2) << "node " << my_node_.id << " has a container id " << my_container_id;
        if (Postoffice::Get()->is_server()) {
            //Send report_eth message
            //Wait for scheduler node to return report_reply message
            Message msgs;
            msgs.meta.recver = kScheduler;
            msgs.meta.control.cmd = Control::REPORT_ETH;
            msgs.meta.sender = my_node_.id;
            msgs.meta.sender_ = my_node_.id;
            msgs.meta.iters = my_container_id;
            PS_VLOG(2) << "container id is " << msgs.meta.iters;  //such as : node 10 has a container id 2
            for (std::size_t i = 1; i < my_node_group.size(); i++)msgs.meta.control.node.push_back(my_node_group[i]);
            msgs.meta.timestamp = timestamp_++;
            Send(msgs);
            PS_VLOG(2) << "node " << my_node_.id << "send report_eth to scheduler";
            for (auto node:msg->meta.control.node) {
                if (node.id == (my_node_.id + 1)) {
                    std::string addr_str = node.hostname + ":" + std::to_string(node.port);
                    if (connected_nodes_.find(addr_str) == connected_nodes_.end()) {
                        Connect_Eth(my_node_, node);
                        PS_VLOG(2) << my_node_.ShortDebugString() << "has connected to " << node.id;
                        connected_nodes_[addr_str] = node.id;
                        //ThreadsafeQueue* temp;
                        send_queue_group.push_back(new ThreadsafeQueue());
                        send_queue_map[node.id]=send_queue_group.size()-1;
                        receive_flag.push_back(true);
                        PS_VLOG(2)<<"node "<<my_node_.id<<" adds a send queue to node "<<node.id;
                        break;
                    }
                }
            }
        } else {
            std::string addr_str1 = my_node_.hostname + ":" + std::to_string(my_node_.port);
            if (connected_nodes_.find(addr_str1) == connected_nodes_.end()) {
                Connect_Eth(my_node_, my_node_);
                PS_VLOG(2) << my_node_.ShortDebugString() << "has connected to " << my_node_.id;
                connected_nodes_[addr_str1] = my_node_.id;
            }
            for (auto node:msg->meta.control.node) {
                if (node.id == (my_node_.id - 1)) {
                    std::string addr_str = node.hostname + ":" + std::to_string(node.port);
                    if (connected_nodes_.find(addr_str) == connected_nodes_.end()) {
                        Connect_Eth(my_node_, node);
                        PS_VLOG(2) << my_node_.ShortDebugString() << "has connected to " << node.id;
                        connected_nodes_[addr_str] = node.id;
                        break;
                    }
                }
            }
        }
        PS_VLOG(2) << my_node_.ShortDebugString() << " is connected to others";
        ready_ = true;
    }
}

void Van::ProcessReportEthCommand(Message *msg) {
    std::unique_lock <std::mutex> eth_lk(eth_mu);
    PS_VLOG(2) << "scheduler receive report_eth from node " << msg->meta.sender_;
    num_report_eth++;
    if (num_report_eth <= Postoffice::Get()->num_servers()) {
        int container_id = msg->meta.iters;
        node_container_id[msg->meta.sender_] = container_id;
        int h = 0;
        for (auto eth:msg->meta.control.node) {
            eth_sort[links[container_id - 1][h]].push_back(eth);
            h++;
        }
    }

    if (num_report_eth == Postoffice::Get()->num_servers()) {
        std::unordered_map<int, int>::iterator iter;
        iter = node_container_id.begin();
        while (iter != node_container_id.end()) iter++;
            
        for (int r : Postoffice::Get()->GetNodeIDs(kServerGroup)) {
            int recver_id = r;
            r = node_container_id[r];
            Message back;
            back.meta.control.node.assign(eth_sort[r].begin(), eth_sort[r].end());
            back.meta.recver = recver_id;
            back.meta.timestamp = timestamp_++;
            back.meta.control.cmd = Control::REPORT_REPLY;
            back.meta.sender = my_node_.id;
            back.meta.sender_ = my_node_.id;
            Send(back);
        }
        PS_VLOG(2) << " REPORT REPLY over";
    }
    eth_lk.unlock();
}

void Van::ProcessReportReplyCommand(Message *msg) {
    PS_VLOG(2) << "node " << my_node_.id << " receive report reply from scheduler";
    eth_node.assign(msg->meta.control.node.begin(), msg->meta.control.node.end());//eth_sort
    eth_node.push_back(my_node_);
    num_workers_ = Postoffice::Get()->num_workers();
    num_servers_ = Postoffice::Get()->num_servers();
    for (auto node:eth_node) {
        std::string addr_str = node.hostname + ":" + std::to_string(node.port);
        if (connected_nodes_.find(addr_str) == connected_nodes_.end()) {
            Connect_Eth(my_node_, node);
            PS_VLOG(2) << my_node_.ShortDebugString() << "has connected to " << node.id;
            neighbors.push_back(node.id);
            connected_nodes_[addr_str] = node.id;
            //ThreadsafeQueue* temp;
            send_queue_group.push_back(new ThreadsafeQueue());
            send_queue_map[node.id]=send_queue_group.size()-1;
            receive_flag.push_back(true);
            PS_VLOG(2)<<"node "<<my_node_.id<<" adds a send queue to node "<<node.id;
        }
    }
    PS_VLOG(2) << "network configuration finished!";
    std::unordered_map<int, int>::iterator iter;
    iter = send_queue_map.begin();
    while (iter != send_queue_map.end()) {
        sender_thread_group.push_back(std::unique_ptr<std::thread>(new std::thread(&Van::Sending, this, iter->first)));
        iter++;
        PS_VLOG(2)<<"GO ON!";
        send_flag.push_back(true);
        std::vector<double> temp(6,0);
        speeds.push_back(temp);
    }
    PS_VLOG(2)<<"thread keep sleepping!";
}

void Van::Start(int customer_id) {
    // get scheduler info
    start_mu_.lock();

    if (init_stage == 0) {
        scheduler_.hostname = std::string(CHECK_NOTNULL(Environment::Get()->find("DMLC_PS_ROOT_URI")));
        scheduler_.port = atoi(CHECK_NOTNULL(Environment::Get()->find("DMLC_PS_ROOT_PORT")));
        scheduler_.role = Node::SCHEDULER;
        scheduler_.id = kScheduler;
        is_scheduler_ = Postoffice::Get()->is_scheduler();



        // get my node info
        if (is_scheduler_) {
            gettimeofday(&t1,NULL);
            update_rate = atof(CHECK_NOTNULL(Environment::Get()->find("UPDATE_RATE")));
            PS_VLOG(2)<<"update rate is "<<update_rate;
            enable_random= (atoi(CHECK_NOTNULL(Environment::Get()->find("ENABLE_RANDOM")))==1)?true:false;
            PS_VLOG(2)<<"enable random is "<<enable_random;
            enable_muti_trace= (atoi(CHECK_NOTNULL(Environment::Get()->find("ENABLE_MUTI_TRACE")))==1)?true:false;
            PS_VLOG(2)<<"enable muti trace is "<<enable_muti_trace;
            check_period=atoi(CHECK_NOTNULL(Environment::Get()->find("CHECK_PERIOD")));
            PS_VLOG(2)<<"check period is "<<check_period;
            in_degree=atoi(Environment::Get()->find("IN_DEGREE"));
            PS_VLOG(2)<<"in degree is "<<in_degree;
            num_enable_server=atoi(Environment::Get()->find("NUM_ENABLE_SERVER"));
            PS_VLOG(2)<<"num_enable_server is "<<num_enable_server;

            my_node_ = scheduler_;
            perf_update_flag=true;
            for (int i = 0; i < num_nodes; i++) {
                perf.push_back(0);
                std::vector < std::vector < std::vector < int >> > temp2;
                for (int j = 0; j < num_nodes; j++) {
                    std::vector <std::vector<int>> temp1;
                    temp2.push_back(temp1);
                }
                route.push_back(temp2);
            }
            unsigned seed;
            seed = 2;
            srand(seed);
            for (int i = 0; i < num_nodes; i++) {
                for (int j = 0; j < num_nodes; j++) {
                    if (tps[i][j] != 0) {
                        tps[i][j] = ((float) (rand() % 1000)) / 10;
                        tps[j][i] = tps[i][j];
                    }
                }
            }
            for (int i = 0; i < 14; i++) {
                std::vector <Node> temp;
                eth_sort.push_back(temp);
            }
        } else {
            if (Postoffice::Get()->is_server()) {
                report_rate = atof(CHECK_NOTNULL(Environment::Get()->find("REPORT_RATE")));
                PS_VLOG(2)<<"report rate is "<<report_rate;
                update_rate = atof(CHECK_NOTNULL(Environment::Get()->find("UPDATE_RATE")));
                PS_VLOG(2)<<"update rate is "<<update_rate;
                num_msg_report = atoi(CHECK_NOTNULL(Environment::Get()->find("NUM_MSG_REPORT")));
                PS_VLOG(2)<<"num msg report is "<<num_msg_report;
                second_queue_limit = atoi(CHECK_NOTNULL(Environment::Get()->find("SECOND_QUEUE_LIMIT")));
                PS_VLOG(2)<<"second queue limit is  "<<second_queue_limit;
                size_msg_report=atoi(Environment::Get()->find("SIZE_MSG_REPORT"));
                PS_VLOG(2)<<"size_msg_report is "<<size_msg_report;
                size_cache=atoi(Environment::Get()->find("SIZE_CACHE"));
                PS_VLOG(2)<<"size_cache is "<<size_cache;
                std::vector<std::vector<std::vector<int>>> temp1;
                route.push_back(temp1);
                for (int i = 0; i < num_nodes; i++) {
                    perf.push_back(0);
                    std::vector<int> temp;
                    route2dst.push_back(temp);
                }
            }

            auto role = is_scheduler_ ? Node::SCHEDULER :
                        (Postoffice::Get()->is_worker() ? Node::WORKER : Node::SERVER);
            const char *nhost = Environment::Get()->find("DMLC_NODE_HOST");
            std::string ip;
            if (nhost) ip = std::string(nhost);
            if (ip.empty()) {
                const char *itf = Environment::Get()->find("DMLC_INTERFACE");
                std::string interface;
                if (itf) interface = std::string(itf);
                if (interface.size()) {
                    GetIP(interface, &ip);
                } else {
                    GetAvailableInterfaceAndIP(&interface, &ip);
                }
                CHECK(!interface.empty()) << "failed to get the interface";
            }
            int port = GetAvailablePort();
            const char *pstr = Environment::Get()->find("PORT");
            if (pstr) port = atoi(pstr);
            CHECK(!ip.empty()) << "failed to get ip";
            CHECK(port) << "failed to get a port";
            my_node_.hostname = ip;
            my_node_.role = role;
            my_node_.port = port;
            // cannot determine my id now, the scheduler will assign it later
            // set it explicitly to make re-register within a same process possible
            my_node_.id = Node::kEmpty;
            my_node_.customer_id = customer_id;
            if (my_node_.role == Node::WORKER) {
                my_node_.id_backup = (atoi(Environment::Get()->find("CONTAINER_ID")) - 1) * 2 + 9;
                PS_VLOG(2) << "container is " << atoi(Environment::Get()->find("CONTAINER_ID")) <<
                            " id backup is " << (atoi(Environment::Get()->find("CONTAINER_ID")) - 1) * 2 + 9;
            }
            if (my_node_.role == Node::SERVER) {
                my_node_.id_backup = (atoi(Environment::Get()->find("CONTAINER_ID")) - 1) * 2 + 8;
                PS_VLOG(2) << "container is " << atoi(Environment::Get()->find("CONTAINER_ID")) <<
                            " id backup is " << (atoi(Environment::Get()->find("CONTAINER_ID")) - 1) * 2 + 8;
            }
        }

        my_node_group.push_back(my_node_);
        // bind.
        my_node_.port = Bind(my_node_, is_scheduler_ ? 0 : 40);
        PS_VLOG(1) << "Bind to " << my_node_.DebugString();
        CHECK_NE(my_node_.port, -1) << "bind failed";

        // connect to the scheduler
        Connect(scheduler_);
        PS_VLOG(2) << my_node_.hostname << " connect to scheduler";

        //inital my veth
        for (int eth = 2; eth <= 13; eth++) {
            if (!is_scheduler_ && eth == 2)continue;
            auto temp = Init_eth_group(eth);
            if (temp.port != Node::kEmpty)my_node_group.push_back(temp);
        }
        PS_VLOG(2) << "inital my veth finished, num_eth is " << my_node_group.size();

        // for debug use
        if (Environment::Get()->find("PS_DROP_MSG")) {
            drop_rate_ = atoi(Environment::Get()->find("PS_DROP_MSG"));
        }
        // start receiver
        StartReceivings();
        init_stage++;
    }
    start_mu_.unlock();

    if (!is_scheduler_) {
        // let the scheduler know myself
        Message msg;
        Node customer_specific_node = my_node_;
        customer_specific_node.customer_id = customer_id;
        msg.meta.recver = kScheduler;
        msg.meta.control.cmd = Control::ADD_NODE;
        msg.meta.control.node.push_back(customer_specific_node);
        msg.meta.timestamp = timestamp_++;
        Send(msg);
    }

    // wait until ready
    while (!ready_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    start_mu_.lock();
    if (init_stage == 1) {
        // resender
        if (Environment::Get()->find("PS_RESEND") && atoi(Environment::Get()->find("PS_RESEND")) != 0) {
            int timeout = 1000;
            if (Environment::Get()->find("PS_RESEND_TIMEOUT")) {
                timeout = atoi(Environment::Get()->find("PS_RESEND_TIMEOUT"));
            }
            resender_ = new Resender(timeout, 10, this);
        }

        if (!is_scheduler_) {
            // start heartbeat thread
            heartbeat_thread_ = std::unique_ptr<std::thread>(
                    new std::thread(&Van::Heartbeat, this));
        }
        init_stage++;
    }
    start_mu_.unlock();
    if (is_scheduler_) {
        while(num_report_eth != Postoffice::Get()->num_servers()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        //enable network awareness, routing policy updates
        route_thread = std::unique_ptr<std::thread>(new std::thread(&Van::Route_Manager, this));
//        route_update();
//        std::this_thread::sleep_for(std::chrono::milliseconds(200));
//        route_update2();
    }
}

Node Van::Init_eth_group(int eth) {
    Node node_;
    auto role = is_scheduler_ ? Node::SCHEDULER :
                (Postoffice::Get()->is_worker() ? Node::WORKER : Node::SERVER);
    std::string ip;
    std::string interface;
    if (ip.empty()) {
        if(Postoffice::Get()->is_scheduler()){
            if(eth<10){
                interface = "toh" + std::to_string(eth);
            }
            else{
                if (Postoffice::Get()->is_scheduler() && eth == 10){eth = 1;interface = "toh" + std::to_string(eth);}
                if (Postoffice::Get()->is_scheduler() && eth == 11){eth = 0;interface = "toh" + std::to_string(eth);}
                if (Postoffice::Get()->is_scheduler() && eth == 12){eth = 2;interface = "tohe" + std::to_string(eth);} //之前没有注释掉
                if (Postoffice::Get()->is_scheduler() && eth == 13){eth = 3;interface = "tohe" + std::to_string(eth);} //之前没有注释掉
            }
        }
        else{
            interface = "toh" + std::to_string(eth);
        }

        if (interface.size()) {
            GetIP(interface, &ip);
        } else {
            GetAvailableInterfaceAndIP(&interface, &ip);
        }
        if (ip.empty()) {
            PS_VLOG(2) << "failed to get the interface";
            return node_;
        } else {
            PS_VLOG(2) << "veth: " << interface << " has a ip " << ip;
            for (auto it:my_node_group) {
                if (it.hostname == ip) {
                    PS_VLOG(2) << " this ip is existense";
                    return node_;
                }
            }
        }
    }
    int port = GetAvailablePort();
    const char *pstr = Environment::Get()->find("PORT");
    if (pstr) port = atoi(pstr);
    CHECK(!ip.empty()) << "failed to get ip";
    CHECK(port) << "failed to get a port";
    node_.hostname = ip;
    node_.role = role;
    if (is_scheduler_) {
        node_.port = 9090;
    } else {
        node_.port = port;
    }
    // cannot determine my id now, the scheduler will assign it later
    // set it explicitly to make re-register within a same process possible

    // bind.
    node_.port = Bind(node_, is_scheduler_ ? 0 : 40);
    PS_VLOG(1) << "Bind to " << node_.DebugString();
    CHECK_NE(node_.port, -1) << "bind failed";
    return node_;
}

void Van::Stop() {
    // stop threads
    Message exit;
    exit.meta.control.cmd = Control::TERMINATE;
    exit.meta.recver = my_node_.id;
    // only customer 0 would call this method
    exit.meta.customer_id = 0;
    int ret = SendMsg(exit);
    CHECK_NE(ret, -1);
    for (std::size_t i = 0; i < receiver_thread_group.size(); i++)receiver_thread_group[i]->join();
    for (std::size_t i = 0; i< sender_thread_group.size(); i++)sender_thread_group[i]->join();
    init_stage = 0;
    if (!is_scheduler_) heartbeat_thread_->join();
    if (resender_) delete resender_;
    ready_ = false;
    connected_nodes_.clear();
    shared_node_mapping_.clear();
    send_bytes_ = 0;
    timestamp_ = 0;
    my_node_.id = Meta::kEmpty;
    barrier_count_.clear();
}

void Van::Add_trace(Message &msg) {
    if (msg.meta.dst == Meta::kEmpty)msg.meta.dst = msg.meta.recver;
    PS_VLOG(2)<<"in add trace, dst is : "<<msg.meta.dst;
    msg.meta.control.cmd = Control::REPLY1;
    msg.meta.priority = 0;
    if (msg.meta.sender == Meta::kEmpty)msg.meta.sender = my_node_.id;
    if (msg.meta.sender_ == Meta::kEmpty)msg.meta.sender_ = my_node_.id;
    msg.meta.last_hop=my_node_.id;
    if (Postoffice::Get()->is_worker()) {
        msg.meta.recver = my_node_.id - 1;
    } else {
        if(msg.meta.dst==(my_node_.id+1)){
            msg.meta.recver=msg.meta.dst;
        }else{
            for(size_t i=(route[0][(msg.meta.dst%2?(msg.meta.dst-9):(msg.meta.dst-8))/2][0].size()-1);i>0;i--){
                msg.meta.route.push_back(route[0][(msg.meta.dst%2?(msg.meta.dst-9):(msg.meta.dst-8))/2][0][i]);
            }
            msg.meta.recver=msg.meta.route.back();
        }
    }
    if(msg.meta.route.size()){
        PS_VLOG(2)<<"in add trace, route is "<<msg.meta.route[0];
    }
    else{
        PS_VLOG(2)<<"worker send to server";
    }
    PS_VLOG(2)<<"recver is "<<msg.meta.recver;
}

void  Van::Send_transmit(Message &msg) {
    int send_bytes = SendMsg(msg);
    CHECK_NE(send_bytes, -1);
    send_bytes_ += send_bytes;
    if (resender_) resender_->AddOutgoing(msg);

    if (Postoffice::Get()->verbose() >= 2) {
        PS_VLOG(2) <<" transmittion " <<msg.DebugString();
    }
}

int Van::Send(Message &msg) {
    if ((Postoffice::Get()->is_worker()||Postoffice::Get()->is_server()) && msg.meta.control.cmd == Control::EMPTY)Add_trace(msg);
    int send_bytes = SendMsg(msg);
    CHECK_NE(send_bytes, -1);
    send_bytes_ += send_bytes;
    if (resender_) resender_->AddOutgoing(msg);

    if (Postoffice::Get()->verbose() >= 2) {
        PS_VLOG(2) << msg.DebugString();
    }
    if (msg.meta.control.cmd == Control::REPLY1 && msg.data.size() == 0) {
        msg.meta.recver = msg.meta.dst;
        msg.meta.dst = Meta::kEmpty;
        msg.meta.control.cmd = Control::EMPTY;
        size_t num_nodes = msg.meta.control.node.size();
        for (size_t i = 0; i < num_nodes; i++)msg.meta.control.node.pop_back();
        PS_VLOG(2)<<"a data msg received: "<<msg.DebugString();
    }
    return send_bytes;
}

void Van::StartReceivings() {
    for (std::size_t i = 0; i < my_node_group.size(); i++) {
        receiver_thread_group.push_back(std::unique_ptr<std::thread>(new std::thread(&Van::Receiving, this, i)));
    }
}

void Van::Push(Message& msg) {
    if (Postoffice::Get()->is_server() && (msg.meta.control.cmd == Control::EMPTY ||
                                            (msg.meta.sender_==(my_node_.id+1) && msg.meta.control.cmd==Control::REPLY1))){
        if(msg.meta.control.cmd == Control::EMPTY)Add_trace(msg);
        PS_VLOG(2)<<"after add trace";
        if(msg.meta.dst!=(my_node_.id+1)) {
            int rank=(msg.meta.dst%2)?(msg.meta.dst-9)/2:(msg.meta.dst-8)/2;
            for (size_t i=1;i<route[0][rank].size();i++){
                auto temp=route[0][rank][i][1];
                if (send_queue_group[send_queue_map[msg.meta.recver]]->size() > (2*second_queue_limit) &&
                    send_queue_group[send_queue_map[temp]]->size() < second_queue_limit) {
                    PS_VLOG(2) << "auxiliary path transmission";
                    msg.meta.route.clear();
                    for(size_t j=(route[0][rank][i].size()-1);j>0;j--){
                        msg.meta.route.push_back(route[0][rank][i][j]);
                    }
                    msg.meta.recver=msg.meta.route.back();
                    break;
                }
            }
        }
    }
    std::unique_lock <std::mutex> send_lk(send_mu);
    send_queue_group[send_queue_map[msg.meta.recver]]->QueuePush(msg);
    if(msg.meta.recver!=(my_node_.id+1) && msg.data.size()>1){
        cache_count+=msg.data[1].capacity();
        EnableSendCheck();
    }
    PS_VLOG(2)<<"in push, send queue to node "<<msg.meta.recver<<" size is "<<send_queue_group[send_queue_map[msg.meta.recver]]->size();
    cond_send.notify_all();
}

void Van::Sending(int node_id) {
    PS_VLOG(2)<<"send queue group size is "<<send_queue_group.size()<<" node_id is "<<node_id<<" send_queue_map[node_id] is "<<send_queue_map[node_id];
    while (true) {
        Message msg;
        bool wait_flag=true;
        std::unique_lock <std::mutex> send_lk(send_mu);
        while(send_queue_group[send_queue_map[node_id]]->empty()){
            if(wait_flag){
                PS_VLOG(2)<<"to node"<<node_id<<" send queue keeps waitting";
            }
            wait_flag=false;
            cond_send.wait(send_lk);
        }
        PS_VLOG(2)<<"to node "<<node_id<<" wait over ";
        send_queue_group[send_queue_map[node_id]]->WaitAndPop2(&msg);

        if(msg.data.size()>1 && msg.data[1].capacity()>=(size_t)size_msg_report){
            gettimeofday(&t1,NULL);//Current system time, stored in t1
            msg.meta.start_time=(double)t1.tv_sec+(double)t1.tv_usec/1000000;//Stores the seconds and microseconds representing the time in the speeds array.
            msg.meta.key=-1;
        }
        send_lk.unlock();
        Send(msg);
        if (!msg.meta.control.empty() &&
            msg.meta.control.cmd == Control::TERMINATE) {
            break;
        }
        PS_VLOG(2)<<"in sending, to node "<<node_id<<" queue size is "<<send_queue_group[send_queue_map[node_id]]->size();
    }
}

void Van::Receiving(int rece_eth_id) {
    recovery_nodes.control.cmd = Control::ADD_NODE;

    while (true) {
        Message msg;
        int recv_bytes = RecvMsg(&msg, rece_eth_id);
        // For debug, drop received message
        if (ready_.load() && drop_rate_ > 0) {
            unsigned seed = time(NULL) + my_node_.id;
            if (rand_r(&seed) % 100 < drop_rate_) {
                LOG(WARNING) << "Drop message " << msg.DebugString();
                continue;
            }
        }

        CHECK_NE(recv_bytes, -1);
        recv_bytes_ += recv_bytes;
        if (Postoffice::Get()->verbose() >= 2) {
            PS_VLOG(2) << msg.DebugString();
        }
        // duplicated message
        if (resender_ && resender_->AddIncomming(msg)) continue;

        if (!msg.meta.control.empty()) {
            // control msg
            auto &ctrl = msg.meta.control;
            if (ctrl.cmd == Control::TERMINATE) {
                ProcessTerminateCommand();
                break;
            } else if (ctrl.cmd == Control::ADD_NODE) {
                ProcessAddNodeCommand(&msg, &nodes, &recovery_nodes);
            } else if (ctrl.cmd == Control::BARRIER) {
                ProcessBarrierCommand(&msg);
            } else if (ctrl.cmd == Control::HEARTBEAT) {
                ProcessHearbeat(&msg);
            } else if (ctrl.cmd == Control::SPEED_REPORT) {
                ProcessSpeedReportCommand(msg);
            } else if (ctrl.cmd == Control::RECV_REPLY) {
                ProcessRecvReplyCommand(msg);
            } else if (ctrl.cmd == Control::ROUTE_RELEASE) {
                ProcessRouteReleaseCommand(msg);
            } else if (ctrl.cmd == Control::REPLY1) {
                if(msg.meta.last_hop>0 && !(msg.meta.last_hop%2) && Postoffice::Get()->is_server())RecvReply(msg);
                if (msg.meta.dst != my_node_.id) {
                    ProcessReply1Command(msg);
                } else {
                    ProcessDataMsg(&msg);
                }
            } else if (ctrl.cmd == Control::REPLY2) {
                if(!(msg.meta.last_hop%2) && Postoffice::Get()->is_server())RecvReply(msg);
                ProcessReply2Command(msg);
            } else if (ctrl.cmd == Control::REPORT_ETH) {
                ProcessReportEthCommand(&msg);
            } else if (ctrl.cmd == Control::REPORT_REPLY) {
                ProcessReportReplyCommand(&msg);
            } else {
                LOG(WARNING) << "Drop unknown typed message " << msg.DebugString();
            }
        } else {
            ProcessDataMsg(&msg);
        }
    }
}

void Van::ProcessReply2Command(Message& msg){
    PS_VLOG(2)<<"in server process reply2 ";
    for(auto son:sons[(msg.meta.sender_-8)/2]){
        Message rpl;
        rpl.meta.app_id=msg.meta.app_id;
        rpl.meta.customer_id=msg.meta.customer_id;
        rpl.meta.request=msg.meta.request;
        rpl.meta.push=msg.meta.push;
        rpl.meta.head =msg.meta.head;
        rpl.meta.timestamp=msg.meta.timestamp;
        rpl.meta.recver= son;
        rpl.meta.sender=msg.meta.sender;
        rpl.meta.sender_=msg.meta.sender_;
        rpl.meta.tree.push_back(-1);
        rpl.meta.key_data=msg.meta.key_data;
        for(auto it:msg.data){
            rpl.AddData(it);
        }
        PS_VLOG(2)<<"server response worker "<<rpl.meta.recver;
        Push(rpl);
    }
}

void Van::ProcessSpeedReportCommand(Message& msg){
    std::unique_lock <std::mutex> send_lk(send_mu);
    int node_rank_id=(msg.meta.last_hop-8)/2;
    for(size_t i=0;i<msg.meta.route.size();i++){
        if(msg.meta.route[i]){
            PS_VLOG(2)<<"to node "<<msg.meta.sender_<<" has a speed "<<msg.meta.route[i];
        }
        if(msg.meta.route[i]==0){
            continue;
        }else{
            tps[node_rank_id][(msg.meta.sender_-8)/2]=msg.meta.route[i];
        }
    }
    change_flag=true;
    PS_VLOG(2)<<"ProcessSpeedReport over";
}

void Van:: ProcessReply1Command(Message& msg){
    PS_VLOG(2)<<"in process reply1 conmmand ";
    msg.meta.last_hop=my_node_.id;
    if(msg.meta.route.size()==0){
        if(msg.meta.iters && msg.meta.push && msg.meta.request){
            if(msg.meta.iters!=iters && !init_flag){
                iters=msg.meta.iters;
                RelationUpdate();
            }
            msg.meta.dst = msg.meta.recver;
            ProcessDataMsg(&msg);
            return;
        }else{
            for(size_t i=(route[0][(msg.meta.dst-8)/2][0].size()-1);i>0;i--){
                msg.meta.route.push_back(route[0][(msg.meta.dst-8)/2][0][i]);
            }
            msg.meta.recver=msg.meta.route.back();//fetch from the end
            PS_VLOG(2)<<"server help worker to send, route is "<<msg.meta.route[0]<<" recver is "<<msg.meta.recver<<" to dst "<<msg.meta.dst;
        }
    }else if(msg.meta.dst==(my_node_.id+1)){
        if(msg.meta.tree.size()){
            ProcessReply2Command(msg);
            return;
        }else {
            msg.meta.recver = msg.meta.dst;
            PS_VLOG(2) << "server send msg to its worker " << msg.meta.recver;
        }
    }else{
        msg.meta.route.pop_back();
        msg.meta.recver=msg.meta.route.back();
        PS_VLOG(2)<<"server "<< msg.meta.sender<<" transmit a msg to node "<<msg.meta.recver<<" to dst "<<msg.meta.dst<<" by trace "<<msg.meta.route[0];
    }
    Push(msg);
}

void Van::ProcessRecvReplyCommand(Message& msg){
    std::unique_lock <std::mutex> send_lk(send_mu);

    PS_VLOG(2)<<"cache count change";

    if(msg.meta.route.size()){
        cache_count-=msg.meta.route[0];
    }
    EnableSendCheck();
    if( msg.meta.key==-1){
        gettimeofday(&t1,NULL);
        double time_temp=(double)t1.tv_sec+(double)t1.tv_usec/1000000-speeds[send_queue_map[msg.meta.sender_]][2];
        PS_VLOG(2)<<"RecvReply's start time is " <<speeds[send_queue_map[msg.meta.sender_]][2]<<" time temp is "<<time_temp<<" speed temp 5 is "<<speeds[send_queue_map[msg.meta.sender_]][5];
        speeds[send_queue_map[msg.meta.sender_]][1]+=time_temp;
        speeds[send_queue_map[msg.meta.sender_]][3]++;
        speeds[send_queue_map[msg.meta.sender_]][0]+=speeds[send_queue_map[msg.meta.sender_]][4];

        PS_VLOG(2)<<"to node"<<msg.meta.sender_<<" data size is "<<speeds[send_queue_map[msg.meta.sender_]][0]<<"time is "<<speeds[send_queue_map[msg.meta.sender_]][1];
        if(speeds[send_queue_map[msg.meta.sender_]][3]==num_msg_report){
            speeds[send_queue_map[msg.meta.sender_]][0]/=speeds[send_queue_map[msg.meta.sender_]][1];
            if((
                    (
                        (speeds[send_queue_map[msg.meta.sender_]][0]-speeds[send_queue_map[msg.meta.sender_]][5])>0?
                        (speeds[send_queue_map[msg.meta.sender_]][0]-speeds[send_queue_map[msg.meta.sender_]][5]):
                        (speeds[send_queue_map[msg.meta.sender_]][5]-speeds[send_queue_map[msg.meta.sender_]][0])
                    )
                    /speeds[send_queue_map[msg.meta.sender_]][5])
                >report_rate){
                PS_VLOG(2)<<"send to scheduler ";
                Message rpt;
                rpt.meta.sender=my_node_.id;
                rpt.meta.sender_=my_node_.id;
                rpt.meta.control.cmd=Control::SPEED_REPORT;
                rpt.meta.recver=1;
                rpt.meta.last_hop=msg.meta.last_hop;

                for(size_t i=1;i<(speeds.size()-1);i++){
                    if(speeds[i][1]==0 || i!=(size_t)send_queue_map[msg.meta.sender_]){
                        rpt.meta.route.push_back(0);
                    }else{
                        rpt.meta.route.push_back((float)speeds[i][0]);
                        PS_VLOG(2)<<"to node "<<msg.meta.sender_<<" speed is "<<rpt.meta.route.back()<<" count is "<<speeds[i][3];
                    }
                }
                float temp=0;
                for(auto it:rpt.meta.route)temp+=it;
                if(temp!=0) {
                    Send(rpt);
                }
                for(size_t i=0;i<speeds.size();i++){
                    if(i!=(size_t)send_queue_map[msg.meta.sender_])continue;
                    speeds[i][5]=speeds[i][0];
                }
            }
            for(size_t i=0;i<speeds.size();i++){
                if(i!=(size_t)send_queue_map[msg.meta.sender_])continue;
                speeds[i][0]=0;
                speeds[i][1]=0;
                speeds[i][3]=0;
            }
        }
    }
    cond_send.notify_all();
}
void Van::EnableSendCheck(){
    if(cache_count>size_cache){
        if(enable_send!=0)enable_send=0;
    }else{
        if(enable_send!=1 && cache_count<(size_cache/5)){
            enable_send=1;
            for(auto node:eth_node){
                if(node.id==my_node_.id || receive_flag[send_queue_map[node.id]])continue;
                Message rpl;
                rpl.meta.sender=my_node_.id;
                rpl.meta.sender_=my_node_.id;
                rpl.meta.last_hop=my_node_.id;
                rpl.meta.control.cmd=Control::RECV_REPLY;
                rpl.meta.recver=node.id;
                rpl.meta.key=-2;
                rpl.meta.iters=enable_send;
                Send(rpl);
                PS_VLOG(2)<<"to node "<<node.id<<" restart send ";
                receive_flag[send_queue_map[rpl.meta.recver]]=true;
            }
        }
    }
}

void Van::RecvReply(Message& msg){
    if(msg.data.size()>1 && msg.meta.key==-1){
        Message rpl;
        rpl.meta.sender=msg.meta.last_hop;
        rpl.meta.sender_=msg.meta.last_hop;
        rpl.meta.last_hop=msg.meta.last_hop;
        rpl.meta.recver=msg.meta.recver;
        if(msg.data.size()>1){
            rpl.meta.route.push_back((float)msg.data[1].capacity());
        }
        if(msg.meta.key==-1){
            rpl.meta.key=-1;
        }
        rpl.meta.iters=enable_send;
        if(!rpl.meta.iters){
            receive_flag[send_queue_map[rpl.meta.recver]]=false;
            PS_VLOG(2)<<" node "<<rpl.meta.recver<<" will stop send";
        }
        speeds[send_queue_map[rpl.meta.sender_]][4]=(msg.data[1].capacity()/1000000.0);
        speeds[send_queue_map[rpl.meta.sender_]][2]=msg.meta.start_time;
        PS_VLOG(2)<<"sending's start time is "<<speeds[send_queue_map[rpl.meta.sender_]][2];
        ProcessRecvReplyCommand(rpl);
    }else{
        Message rpl;
        rpl.meta.sender=my_node_.id;
        rpl.meta.sender_=my_node_.id;
        rpl.meta.last_hop=my_node_.id;
        rpl.meta.control.cmd=Control::RECV_REPLY;
        rpl.meta.recver=msg.meta.last_hop;
        PS_VLOG(2)<<"recv reply, sender is "<<rpl.meta.sender_<<", recver is "<<rpl.meta.recver;
        if(msg.data.size()>1){
            rpl.meta.route.push_back((float)msg.data[1].capacity());
        }
        if(msg.meta.key==-1)rpl.meta.key=-1;
        rpl.meta.iters=enable_send;
        if(!rpl.meta.iters){
            receive_flag[send_queue_map[rpl.meta.recver]]=false;
            PS_VLOG(2)<<" node "<<rpl.meta.recver<<" will stop send";
        }
        Send(rpl);
    }
}

void Van::PackMeta(const Meta& meta, char** meta_buf, int* buf_size) {
    // convert into protobuf
    PBMeta pb;
    pb.set_head(meta.head);
    if (meta.sender_ != Meta::kEmpty) pb.set_sender_(meta.sender_);
    if (meta.app_id != Meta::kEmpty) pb.set_app_id(meta.app_id);
    if (meta.timestamp != Meta::kEmpty) pb.set_timestamp(meta.timestamp);
    if(meta.version != Meta::kEmpty) pb.set_version(meta.version);
    if (meta.key != Meta::kEmpty) pb.set_key(meta.key);
    if (meta.iters != Meta::kEmpty) pb.set_iters(meta.iters);
    if (meta.dst != Meta::kEmpty) pb.set_dst(meta.dst);
    if (meta.priority != Meta::kEmpty) pb.set_priority(meta.priority);
    if (meta.last_hop != Meta::kEmpty) pb.set_last_hop(meta.last_hop);
    if (meta.body.size()) pb.set_body(meta.body);
    pb.set_push(meta.push);
    pb.set_request(meta.request);
    pb.set_simple_app(meta.simple_app);
    pb.set_customer_id(meta.customer_id);
    pb.set_key_data(meta.key_data);
    pb.set_start_time(meta.start_time);
    for (auto d : meta.data_type) pb.add_data_type(d);
    for (auto d : meta.route) pb.add_route(d);
    for (auto d : meta.tree) pb.add_tree(d);
    if (!meta.control.empty()) {
        auto ctrl = pb.mutable_control();
        ctrl->set_cmd(meta.control.cmd);
        if (meta.control.cmd == Control::BARRIER) {
            ctrl->set_barrier_group(meta.control.barrier_group);
        } else if (meta.control.cmd == Control::ACK) {
            ctrl->set_msg_sig(meta.control.msg_sig);
        }
        for (const auto& n : meta.control.node) {
            auto p = ctrl->add_node();
            p->set_id(n.id);
            p->set_role(n.role);
            p->set_port(n.port);
            p->set_hostname(n.hostname);
            p->set_is_recovery(n.is_recovery);
            p->set_customer_id(n.customer_id);
            p->set_id_backup(n.id_backup);
        }
    }

    // to string
    *buf_size = pb.ByteSize();
    *meta_buf = new char[*buf_size+1];
    CHECK(pb.SerializeToArray(*meta_buf, *buf_size))
            << "failed to serialize protbuf";
}

void Van::UnpackMeta(const char* meta_buf, int buf_size, Meta* meta) {
    // to protobuf
    PBMeta pb;
    CHECK(pb.ParseFromArray(meta_buf, buf_size))
            << "failed to parse string into protobuf";

    // to meta
    meta->sender_ = pb.has_sender_() ? pb.sender_() : Meta::kEmpty;
    meta->key = pb.has_key() ? pb.key() : Meta::kEmpty;
    meta->version = pb.has_version() ? pb.version() : Meta::kEmpty;
    meta->iters = pb.has_iters() ? pb.iters() : Meta::kEmpty;
    meta->dst = pb.has_dst() ? pb.dst() : Meta::kEmpty;
    meta->priority = pb.has_priority() ? pb.priority() : Meta::kEmpty;
    meta->last_hop = pb.has_last_hop() ? pb.last_hop() : Meta::kEmpty;
    meta->head = pb.head();
    meta->app_id = pb.has_app_id() ? pb.app_id() : Meta::kEmpty;
    meta->timestamp = pb.has_timestamp() ? pb.timestamp() : Meta::kEmpty;
    meta->request = pb.request();
    meta->push = pb.push();
    meta->simple_app = pb.simple_app();
    meta->body = pb.body();
    meta->customer_id = pb.customer_id();
    meta->key_data = pb.key_data();
    meta->start_time = pb.start_time();
    meta->route.resize(pb.route_size());
    meta->tree.resize(pb.tree_size());
    for(int i=0;i<pb.route_size();++i){
        meta->route[i]=static_cast<float>(pb.route(i));
    }
    for(int i=0;i<pb.tree_size();++i){
        meta->tree[i]=static_cast<float>(pb.tree(i));
    }
    meta->data_type.resize(pb.data_type_size());
    for (int i = 0; i < pb.data_type_size(); ++i) {
        meta->data_type[i] = static_cast<DataType>(pb.data_type(i));
    }
    if (pb.has_control()) {
        const auto& ctrl = pb.control();
        meta->control.cmd = static_cast<Control::Command>(ctrl.cmd());
        meta->control.barrier_group = ctrl.barrier_group();
        meta->control.msg_sig = ctrl.msg_sig();
        for (int i = 0; i < ctrl.node_size(); ++i) {
            const auto& p = ctrl.node(i);
            Node n;
            n.role = static_cast<Node::Role>(p.role());
            n.port = p.port();
            n.hostname = p.hostname();
            n.id = p.has_id() ? p.id() : Node::kEmpty;
            n.is_recovery = p.is_recovery();
            n.customer_id = p.customer_id();
            n.id_backup = p.has_id_backup() ? p.id_backup() : Node::kEmpty;
            meta->control.node.push_back(n);
        }
    } else {
        meta->control.cmd = Control::EMPTY;
    }
}

void Van::Heartbeat() {
    const char* val = Environment::Get()->find("PS_HEARTBEAT_INTERVAL");
    const int interval = val ? atoi(val) : kDefaultHeartbeatInterval;
    while (interval > 0 && ready_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        Message msg;
        msg.meta.recver = kScheduler;
        msg.meta.control.cmd = Control::HEARTBEAT;
        msg.meta.control.node.push_back(my_node_);
        msg.meta.timestamp = timestamp_++;
        Send(msg);
    }
}

bool Van::i_in_trace(int i, std::vector<int> trace) {
    for (auto it : trace) {
        if (it == i)
            return true;
    }
    return false;
}

int Van::x_in_array(int x, std::vector<int> array){
    for(size_t i=0;i<array.size();i++){
        if(array[i]==x)
            return (int)i;
    }
    return -1;
}

void Van::find_trace(int start, int end, float max_tp, std::vector<int> trace) {
    for (int i = 0; i < num_nodes; i++) {
        if (i == start || tps[start][i] == 0) {
            continue;
        }else {
            if (i == end) {
                trace.push_back(i);
                float temp = max_tp;
                max_tp += (1 / tps[start][i]);
                max_tps.push_back(max_tp);
                traces.push_back(trace);
                trace.pop_back();
                max_tp = temp;
                continue;
            }
            else {
                if (i_in_trace(i, trace)) {
                    continue;
                }else {
                    trace.push_back(i);
                    float temp = max_tp;
                    max_tp += (1 / tps[start][i]);
                    find_trace(i, end, max_tp, trace);
                    trace.pop_back();
                    max_tp = temp;
                }
            }
        }
    }
}

int Van::min(std::vector<float> max_tps) {
    int index=-1;
    float min_=100000000;
    for (size_t i = 0; i < max_tps.size();i++) {
        if (max_tps[i] < min_) {
            min_ = max_tps[i];
            index = i;
        }
    }
    return index;
}

void Van::check_trace(){
    int temp=0;
    for(auto it:traces){
        temp++;
    }
    size_t min_index = min(max_tps);
    for(size_t i=0;i<traces.size();i++){
        if(i==min_index){
            continue;
        }else{
            for(size_t j=1;j<(traces[min_index].size()-1);j++){
                if(i_in_trace(traces[min_index][j],traces[i])){
                    break;
                }
            }
        }
    }
}

void Van::add_route() {
    //route initialize
    std::vector<std::vector<std::vector<std::vector<int>>>> temp;
    for(int i=0;i<num_nodes;i++){
        std::vector<std::vector<std::vector<int>>> temp1;
        for(int j=0;j<num_nodes;j++){
            std::vector<std::vector<int>> temp;
            temp1.push_back(temp);
        }
        temp.push_back(temp1);
    }
    route.swap(temp);

    std::vector<std::vector<bool>> log;
    for (int i = 0; i < num_nodes; i++) {
        std::vector<bool> temp(num_nodes, false);
        log.push_back(temp);
    }
    srand((unsigned int)(time(NULL)));
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            if (!log[i][j] && i!=j) {
                max_tps.clear();
                traces.clear();
                mid_nodes.clear();
                std::vector<int> trace;
                trace.push_back(i);
                find_trace(i, j, 0, trace);
                for (size_t k = 0; k < (enable_muti_trace?traces.size():1); k++) {
                    int min_index = enable_random?(int)(rand()%traces.size()):min(max_tps);
                    if(max_tps[min_index]==1000000){
                        route[i][j].push_back(std::vector<int>());
                    }else{
                        bool flag=false;
                        for(size_t h=1;h<(traces[min_index].size()-1);h++){
                            if(i_in_trace(traces[min_index][h],mid_nodes)){
                                max_tps[min_index] = 1000000;
                                route[i][j].push_back(std::vector<int>());
                                flag=true;
                                break;
                            }
                        }
                        if(flag)continue;
                        route[i][j].push_back(traces[min_index]);
                        max_tps[min_index] = 1000000;
                        for(size_t h=1;h<(traces[min_index].size()-1);h++){
                            if(!i_in_trace(traces[min_index][h],mid_nodes)){
                                mid_nodes.push_back(traces[min_index][h]);
                            }
                        }
                    }
                }
                log[i][j] = true;
            }
        }
    }
}

void Van::compute_performance() {
    std::vector<std::vector<float>> tp;
    std::vector<std::vector<bool>> log;
    std::vector<std::vector<bool>> log_global;
    std::vector<int> log_count;

    for (int k = 0; k < num_nodes; k++) {
        std::vector<bool> temp(num_nodes, false);
        log.push_back(temp);
        log_global.push_back(temp);
        log_count.push_back(0);
    }
    std::vector<float> perf_temp;
    perf_temp.assign(perf.begin(),perf.end());
    for (int i = 0; i < num_nodes; i++) {
        tp.clear();
        for(int i =0;i<num_nodes;i++){
            for(int j=0;j<num_nodes;j++){
                if(log[i][j])
                    log[i][j]=false;
            }
        }
        for (int j = 0; j < num_nodes; j++) {
            if (i == j)continue;
            if (route[i][j][0].size() > tp.size()) {
                while (route[i][j][0].size() > tp.size()) {
                    std::vector<float> temp;
                    tp.push_back(temp);
                }
            }
            for (size_t k = 0; k < (route[i][j][0].size()-1); k++) {
                if (!log[route[i][j][0][k]][route[i][j][0][k + 1]]) {
                    tp[k].push_back( tps[route[i][j][0][k]][route[i][j][0][k+1]]);
                    log[route[i][j][0][k]][route[i][j][0][k + 1]]=true;
                    if(!log_global[route[i][j][0][k]][route[i][j][0][k + 1]]){
                        log_global[route[i][j][0][k]][route[i][j][0][k + 1]]=true;
                        log_count[i]++;
                    }
                }
            }
        }

        perf[i]=0;
        for (size_t k = 0; k < tp.size(); k++) {
            if(tp[k].size()) {
                perf[i] += 1/tp[k][min(tp[k])];
            }
        }
        perf[i]=1/perf[i];
    }

    //choose the former max nodes
    for(int i=0;i<(num_nodes-num_enable_server);i++){
        perf[min(perf)]=Meta::kEmpty;
    }

    for(size_t i=0;i<perf.size();i++){
        if(perf[i]==Meta::kEmpty){
            perf[i]=0;
        }
    }
}

float Van::check_change(float temp[9][9]){
    std::unique_lock <std::mutex> send_lk(send_mu);

    //choose the ave
    int num=0;
    float diff=0;
    for(int i=0;i<num_nodes;i++){
        for(int j=0;j<num_nodes;j++){
            if(tps[i][j]!=temp[i][j]){
                num++;
                diff+=(((tps[i][j]-temp[i][j])>0?(tps[i][j]-temp[i][j]):(temp[i][j]-tps[i][j]))/temp[i][j]);
            }
        }
    }
    PS_VLOG(2)<<"changed num is "<<num<<"diff is "<<diff<<" average diff is "<<diff/num;
    return diff/num;
}

void Van::Route_Manager(){
    if(is_scheduler_){
        while(1){
            route_update();
            if(init_flag){
                init_flag=false;
            }
            float temp[9][9];
            for(int i=0;i<num_nodes;i++){
                for(int j=0;j<num_nodes;j++){
                    temp[i][j]=tps[i][j];
                }
            }
            while(1){
                std::this_thread::sleep_for(std::chrono::seconds(check_period));
                if(check_change(temp)>update_rate){
                    break;
                }
            }
        }
    }
}

void Van::route_update(){
    //add dynamic route thread
    add_route();

    compute_performance();
    perf_update_flag=true;

    for (int r : Postoffice::Get()->GetNodeIDs(kServerGroup)) {
        int recver_id = r;
        r=node_container_id[r]-1;
        Message back;
        back.meta.recver = recver_id;
        back.meta.timestamp = timestamp_++;
        back.meta.control.cmd = Control::ROUTE_RELEASE;
        back.meta.sender = my_node_.id;
        back.meta.sender_ = my_node_.id;
        back.meta.iters=0;
        back.meta.route.push_back(mid_tp);

        for(int j=0;j<num_nodes;j++){
            back.meta.route.push_back(-2);
            for(size_t k=0;k<route[r][j].size();k++){
                if(!route[r][j][k].size())continue;
                for(auto it:route[r][j][k])
                    back.meta.route.push_back((float)it);
                back.meta.route.push_back(-1);
            }
        }
        Send(back);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Van::route_update2(){
    auto policy=PolicyDecision();
    int s=0;
    for (int r : Postoffice::Get()->GetNodeIDs(kServerGroup)) {
        Message back;
        back.meta.recver = r;
        back.meta.control.cmd = Control::ROUTE_RELEASE;
        back.meta.sender = my_node_.id;
        back.meta.sender_ = my_node_.id;
        back.meta.route.assign(policy[s].begin(),policy[s].end());
        back.meta.iters = 1;
        Send(back);
        s++;
    }
    perf_update_flag=false;
}

std::vector<std::vector<float>> Van::PolicyDecision(){
    std::vector<std::vector<float>> policy;

    //write server perf
    for(int i=0;i<num_nodes;i++){
        std::vector<float> temp;
        temp.assign(perf.begin(),perf.end());
        policy.push_back(temp);
    }

    //init next hop list
    std::vector<std::vector<int>> next_hop_list;
    for(int i=0;i<num_nodes;i++){
        std::vector<int> temp;
        next_hop_list.push_back(temp);
    }

    //write next hop list
    for(int i=0;i<num_nodes;i++){
        for(int j=0;j<num_nodes;j++){
            if(i==j){
                next_hop_list[i].push_back(-2);
            }else{
                next_hop_list[i].push_back(route[i][j][0][1]);
            }
        }
    }

    //write policy
    for(size_t i=0;i<perf.size();i++){
        if(perf[i]){
            for(int j=0;j<num_nodes;j++){
                policy[j].push_back((float)next_hop_list[j][i]);//with i as the root node, j goes to the next hop of i, i.e., the parent node of j.
                for(int k=0;k<num_nodes;k++){
                    if(next_hop_list[k][i]==j){
                        policy[j].push_back(k);//with i as the root node and j's children
                    }
                }
                policy[j].push_back(-1);
            }
        }
    }
    return policy;
}

void Van::ProcessRouteReleaseCommand(Message& msg){
    if(is_scheduler_){
        std::unique_lock <std::mutex> send_lk(send_mu);
        notify_count++;
        if(notify_count==num_nodes){
            notify_count=0;
        }else{
            return;
        }

        if(perf_update_flag){
            route_update2();
            perf_update_flag=false;
        }else{
            for (int r : Postoffice::Get()->GetNodeIDs(kServerGroup)) {
                Message back;
                back.meta.recver = r;
                back.meta.control.cmd = Control::ROUTE_RELEASE;
                back.meta.sender = my_node_.id;
                back.meta.sender_ = my_node_.id;
                back.meta.iters = 1;
                Send(back);
            }
            perf_update_flag=false;
        }
        gettimeofday(&t2,NULL);
        PS_VLOG(2)<<"last iter time is "<<(float)(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000;
        gettimeofday(&t1,NULL);
        send_lk.unlock();
    }else if (Postoffice::Get()->is_server()) {
        if(msg.meta.iters) {
            msg.meta.recver++;
            msg.meta.dst++;
            Send(msg);
            if(!msg.meta.route.size()){
                return;
            }
            parents_temp.clear();
            sons_temp.clear();
            perf.clear();
            perf.assign(msg.meta.route.begin(), msg.meta.route.begin() + num_nodes);

            int i = 0;
            size_t j = num_nodes;
            //parsing routing information
            while (j < msg.meta.route.size()) {
                if (perf[i]) {
                    if(msg.meta.route[j]==-2){
                        parents_temp.push_back(my_node_.id);
                    }else{
                        parents_temp.push_back((int) msg.meta.route[j]*2+8);
                    }
                    std::vector<int> temp;
                    sons_temp.push_back(temp);
                    j++;
                    while (msg.meta.route[j] != -1) {
                        sons_temp[i].push_back((int) msg.meta.route[j]*2+9);
                        j++;
                    }
                    j++;
                } else {
                    parents_temp.push_back(-1);
                    std::vector<int> temp;
                    sons_temp.push_back(temp);
                }
                i++;
            }

            while (parents_temp.size() < (size_t) num_nodes) {
                parents_temp.push_back(-1);
                std::vector<int> temp;
                sons_temp.push_back(temp);
            }

            for(int i=0;i<num_nodes;i++){
                sons_temp[i].push_back(my_node_.id+1);
            }

            init_flag=false;
        }else{
            std::vector<std::vector<std::vector<int>>> temp;
            size_t row=0;
            size_t col=1;
            for (size_t i = 1; i < msg.meta.route.size(); i++) {
                if(msg.meta.route[i]==-2){
                    row++;
                    col=1;
                }else if(msg.meta.route[i]==-1){
                    col++;
                }else{
                    while(row>temp.size()){
                        std::vector<std::vector<int>> temp2;
                        temp.push_back(temp2);
                    }
                    while(col>temp[row-1].size()){
                        std::vector<int> temp3;
                        temp[row-1].push_back(temp3);
                    }
                    temp[row-1][col-1].push_back((int)(msg.meta.route[i]*2+8));
                }
            }
            std::unique_lock <std::mutex> send_lk(send_mu);
            mid_tp=msg.meta.route[0];
            route[0].swap(temp);//update route0
        }
    }else{
        if(msg.meta.route.size()){
            perf.assign(msg.meta.route.begin(),msg.meta.route.begin()+num_nodes);
            PS_VLOG(2)<<"it's worker route release, and perf is : ";
            for(auto it:perf)std::cout<<it<<" ";std::cout<<std::endl;
            init_flag=false;
        }else{
            init_flag=true;
        }
        perf_update_flag=true;
    }
}

void Van::RelationUpdate(){
    parents.clear();
    parents.assign(parents_temp.begin(),parents_temp.end());
    sons.clear();
    for(auto it:sons_temp){
        sons.push_back(it);
    }
}

std::vector<int> Van::MulticastTreeBuild(){
    std::unique_lock <std::mutex> send_lk(send_mu);
    std::vector<std::vector<int>>  topo;
    std::vector<std::vector<int>> topo_last_hop;
    for(size_t i=0;i<route[0].size();i++){
        if(!route[0][i].size())continue;
        while(route[0][i][0].size()>topo.size()){
            std::vector<int> temp1;
            topo.push_back(temp1);
            std::vector<int> temp2;
            topo_last_hop.push_back(temp2);
        }
        for(size_t j=1;j<route[0][i][0].size();j++){
            if(!i_in_trace(route[0][i][0][j],topo[j])){
                topo[j].push_back(route[0][i][0][j]);
                topo_last_hop[j].push_back(route[0][i][0][j-1]);
            }
        }
    }
    std::vector<int> tree;
    for(int i=(topo.size()-1);i>=0;i--){
        for(size_t j=0;j<topo[i].size();j++){
            tree.push_back(topo[i][j]);
            tree.push_back(topo_last_hop[i][j]);
        }
        tree.push_back(-1);
    }
    tree.pop_back();
    tree.pop_back();

    return tree;
}
}  // namespace ps
