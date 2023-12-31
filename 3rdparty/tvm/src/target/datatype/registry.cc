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
#include <tvm/runtime/registry.h>
#include "registry.h"

namespace tvm {
namespace datatype {

using runtime::TVMArgs;
using runtime::TVMRetValue;

TVM_REGISTER_GLOBAL("_datatype_register")
.set_body([](TVMArgs args, TVMRetValue* ret) {
  datatype::Registry::Global()->Register(args[0], static_cast<uint8_t>(args[1].operator int()));
});

TVM_REGISTER_GLOBAL("_datatype_get_type_code")
.set_body([](TVMArgs args, TVMRetValue* ret) {
  *ret = datatype::Registry::Global()->GetTypeCode(args[0]);
});

TVM_REGISTER_GLOBAL("_datatype_get_type_name")
.set_body([](TVMArgs args, TVMRetValue* ret) {
  *ret = Registry::Global()->GetTypeName(args[0].operator int());
});

TVM_REGISTER_GLOBAL("_datatype_get_type_registered")
.set_body([](TVMArgs args, TVMRetValue* ret) {
  *ret = Registry::Global()->GetTypeRegistered(args[0].operator int());
});

Registry* Registry::Global() {
  static Registry inst;
  return &inst;
}

void Registry::Register(const std::string& type_name, uint8_t type_code) {
  CHECK(type_code >= kTVMCustomBegin)
      << "Please choose a type code >= kTVMCustomBegin for custom types";
  code_to_name_[type_code] = type_name;
  name_to_code_[type_name] = type_code;
}

uint8_t Registry::GetTypeCode(const std::string& type_name) {
  CHECK(name_to_code_.find(type_name) != name_to_code_.end())
      << "Type name " << type_name << " not registered";
  return name_to_code_[type_name];
}

std::string Registry::GetTypeName(uint8_t type_code) {
  CHECK(code_to_name_.find(type_code) != code_to_name_.end())
      << "Type code " << static_cast<unsigned>(type_code) << " not registered";
  return code_to_name_[type_code];
}

const runtime::PackedFunc* GetCastLowerFunc(const std::string& target, uint8_t type_code,
                                            uint8_t src_type_code) {
  std::ostringstream ss;
  ss << "tvm.datatype.lower.";
  ss << target << ".";
  ss << "Cast"
     << ".";

  if (datatype::Registry::Global()->GetTypeRegistered(type_code)) {
    ss << datatype::Registry::Global()->GetTypeName(type_code);
  } else {
    ss << runtime::TypeCode2Str(type_code);
  }

  ss << ".";

  if (datatype::Registry::Global()->GetTypeRegistered(src_type_code)) {
    ss << datatype::Registry::Global()->GetTypeName(src_type_code);
  } else {
    ss << runtime::TypeCode2Str(src_type_code);
  }

  return runtime::Registry::Get(ss.str());
}

const runtime::PackedFunc* GetFloatImmLowerFunc(const std::string& target, uint8_t type_code) {
  std::ostringstream ss;
  ss << "tvm.datatype.lower.";
  ss << target;
  ss << ".FloatImm.";
  ss << datatype::Registry::Global()->GetTypeName(type_code);
  return runtime::Registry::Get(ss.str());
}

uint64_t ConvertConstScalar(uint8_t type_code, double value) {
  std::ostringstream ss;
  ss << "tvm.datatype.convertconstscalar.float.";
  ss << datatype::Registry::Global()->GetTypeName(type_code);
  auto make_const_scalar_func = runtime::Registry::Get(ss.str());
  return (*make_const_scalar_func)(value).operator uint64_t();
}

}  // namespace datatype
}  // namespace tvm
