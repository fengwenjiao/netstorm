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
 * \file pass.cc
 * \brief Support for pass registry.
 */
#include <nnvm/pass.h>
#include <algorithm>

namespace dmlc {
// enable registry
DMLC_REGISTRY_ENABLE(nnvm::PassFunctionReg);
}  // namespace dmlc

namespace nnvm {

const PassFunctionReg* FindPassDep(const std::string&attr_name) {
  for (auto* r : dmlc::Registry<PassFunctionReg>::List()) {
    for (auto& s : r->graph_attr_targets) {
      if (s == attr_name) return r;
    }
  }
  return nullptr;
}

Graph ApplyPasses(Graph g,
                  const std::vector<std::string>& pass) {
  std::vector<const PassFunctionReg*> fpass;
  for (auto& name : pass) {
    auto* reg = dmlc::Registry<PassFunctionReg>::Find(name);
    CHECK(reg != nullptr)
        << "Cannot find pass " << name << " in the registry";
    fpass.push_back(reg);
  }

  for (auto r : fpass) {
    for (auto& dep : r->graph_attr_dependency) {
      if (g.attrs.count(dep) == 0) {
        auto* pass_dep = FindPassDep(dep);
        std::string msg;
        if (pass_dep != nullptr) {
          msg = " The attribute is provided by pass " + pass_dep->name;
        }
        LOG(FATAL) << "Graph attr dependency " << dep
                   << " is required by pass " << r->name
                   << " but is not available "
                   << msg;
      }
    }
    g = r->body(std::move(g));
  }

  return g;
}

}  // namespace nnvm
