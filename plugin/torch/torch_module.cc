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
 * Copyright (c) 2015 by Contributors
 * \file activation.cc
 * \brief activation op
 * \author Bing Xu
*/
#include "./torch_module-inl.h"
#include "../../src/operator/mshadow_op.h"

namespace mxnet {
namespace op {
template<>
Operator *CreateOp<cpu>(TorchModuleParam param, TorchState* torchState) {
  return new TorchModuleOp<cpu>(param, torchState);
}

// DO_BIND_DISPATCH comes from operator_common.h
Operator *TorchModuleProp::CreateOperator(Context ctx) const {
  DO_BIND_DISPATCH(CreateOp, param_, torchState_);
}

DMLC_REGISTER_PARAMETER(TorchModuleParam);

MXNET_REGISTER_OP_PROPERTY(TorchModule, TorchModuleProp)
.describe("Modules from torch.")
.add_arguments(TorchModuleParam::__FIELDS__());

}  // namespace op
}  // namespace mxnet
