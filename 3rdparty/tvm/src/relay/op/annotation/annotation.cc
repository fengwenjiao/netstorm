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
 *
 * \file src/relay/op/annotation/annotation.cc
 * \brief Registration of annotation operators.
 */

#include <tvm/tir/expr.h>
#include <tvm/relay/attrs/annotation.h>
#include <tvm/relay/expr.h>
#include <tvm/relay/op.h>
#include <tvm/relay/op_attr_types.h>
#include <topi/elemwise.h>

#include "../../pass/infer_layout_util.h"
#include "../type_relations.h"

namespace tvm {
namespace relay {

// relay.annotation.on_device
TVM_REGISTER_NODE_TYPE(OnDeviceAttrs);

TVM_REGISTER_GLOBAL("relay.op.annotation._make.on_device")
.set_body_typed([](Expr data, int device_type) {
  auto attrs = make_object<OnDeviceAttrs>();
  attrs->device_type = device_type;
  static const Op& op = Op::Get("on_device");
  return CallNode::make(op, {data}, Attrs(attrs), {});
});

RELAY_REGISTER_OP("on_device")
.describe(R"code(Annotate an expression with device type)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout);

Expr StopFusion(Expr data) {
  static const Op& op = Op::Get("annotation.stop_fusion");
  return CallNode::make(op, {data}, Attrs{}, {});
}

TVM_REGISTER_GLOBAL("relay.op.annotation._make.stop_fusion")
.set_body_typed([](Expr data) {
    return StopFusion(data);
});

RELAY_REGISTER_OP("annotation.stop_fusion")
.describe(R"code(Annotate an expression to prevent it being fused with previous expressions.)code"
TVM_ADD_FILELINE)
.set_num_inputs(1)
.add_argument("data", "Tensor", "The input data.")
.add_type_rel("Identity", IdentityRel)
.set_support_level(10)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });

// relay.annotation.cast_hint
TVM_REGISTER_NODE_TYPE(CastHintAttrs);

Expr CastHint(Expr data, DataType dtype) {
  auto attrs = make_object<CastHintAttrs>();
  attrs->dtype = dtype;
  static const Op& op = Op::Get("annotation.cast_hint");
  return CallNode::make(op, {data}, Attrs{attrs}, {});
}

RELAY_REGISTER_OP("annotation.cast_hint")
.describe(R"code(Annotate an expression to be cast into specific data type.)code"
TVM_ADD_FILELINE)
.set_num_inputs(1)
.add_argument("data", "Tensor", "The input data.")
.add_type_rel("Identity", IdentityRel)
.set_support_level(10)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });


RELAY_REGISTER_OP("annotation.bitpack_start")
.describe(R"code(
Mark the start of bitpacking.
)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });

RELAY_REGISTER_OP("annotation.bitpack_end")
.describe(R"code(
Mark the end of bitpacking.
)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });

TVM_REGISTER_GLOBAL("relay.op.annotation._make.checkpoint")
.set_body_typed([](Expr data) {
  static const Op& op = Op::Get("annotation.checkpoint");
  return CallNode::make(op, {data}, Attrs{}, {});
});

RELAY_REGISTER_OP("annotation.checkpoint")
.describe(R"code(
Mark a checkpoint for checkpointing memory optimization.
)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         Array<te::Tensor> outputs;
                         for (size_t i = 0; i < inputs.size(); ++i) {
                           outputs.push_back(topi::identity(inputs[i]));
                         }
                         return outputs;
                       });

RELAY_REGISTER_OP("annotation.compiler_begin")
.describe(R"code(
Beginning of a region that is handled by a given compiler.
)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });

TVM_REGISTER_GLOBAL("relay.op.annotation._make.compiler_begin")
.set_body_typed([](Expr expr, std::string compiler) {
  auto attrs = make_object<CompilerAttrs>();
  attrs->compiler = compiler;
  static const Op& op = Op::Get("annotation.compiler_begin");
  return CallNode::make(op, {expr}, Attrs(attrs), {});
});

RELAY_REGISTER_OP("annotation.compiler_end")
.describe(R"code(
End of a region that is handled by a given compiler.
)code" TVM_ADD_FILELINE)
.set_num_inputs(1)
.set_support_level(10)
.add_type_rel("Identity", IdentityRel)
.set_attr<TOpPattern>("TOpPattern", kOpaque)
.set_attr<TOpIsStateful>("TOpIsStateful", false)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ElemwiseArbitraryLayout)
.set_attr<FTVMCompute>("FTVMCompute",
                       [](const Attrs& attrs, const Array<te::Tensor>& inputs,
                          const Type& out_dtype, const Target& target) -> Array<te::Tensor> {
                         return {topi::identity(inputs[0])};
                       });

TVM_REGISTER_GLOBAL("relay.op.annotation._make.compiler_end")
.set_body_typed([](Expr expr, std::string compiler) {
  auto attrs = make_object<CompilerAttrs>();
  attrs->compiler = compiler;
  static const Op& op = Op::Get("annotation.compiler_end");
  return CallNode::make(op, {expr}, Attrs(attrs), {});
});

}  // namespace relay
}  // namespace tvm
