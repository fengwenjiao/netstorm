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

/*!
 * \file base.cc
 * \brief The core base types for Relay.
 */

#include <tvm/ir/type.h>
#include <tvm/runtime/registry.h>
#include <tvm/relay/base.h>

namespace tvm {
namespace relay {

using namespace tvm::runtime;

TVM_REGISTER_NODE_TYPE(IdNode);

TVM_REGISTER_GLOBAL("relay._base.set_span")
.set_body_typed([](ObjectRef node_ref, Span sp) {
  if (auto* rn = node_ref.as<RelayNode>()) {
    rn->span = sp;
  } else if (auto* rn = node_ref.as<RelayExprNode>()) {
    rn->span = sp;
  } else if (auto* rn = node_ref.as<TypeNode>()) {
    rn->span = sp;
  } else {
    LOG(FATAL) << "Expect Type or RelayNode ";
  }
});

}  // namespace relay
}  // namespace tvm
