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
 * \file src/relay/qnn/op/requantize.cc
 * \brief QNN requantize operator.
 */

#include <tvm/relay/analysis.h>
#include <tvm/relay/op_attr_types.h>
#include <tvm/relay/qnn/attrs.h>
#include "../../pass/pattern_util.h"
#include "../util.h"

namespace tvm {
namespace relay {
namespace qnn {

TVM_REGISTER_NODE_TYPE(RequantizeAttrs);

// Lowering of qnn.requantize op

/*
 * \brief Lower requantize to a sequence of ops.
 * \param input_tensor The input tensor to requantize op.
 * \param param The requantize op attrs.
 * \param input_shape The input tensor shape of the requantize op.
 * \return The sequence of existing Relay ops.
 * \note Requantization using only integer computation. Here, the computation is
 *       converted to a fixed point computation by computing output multiplier
 *       and shift. This is useful, if the target device does not support/have
 *       very expensive floating point computations.
 *
 *       The whole computation this can be broken down into following steps
 *       1) Calculate the integer multiplier and integer shift.
 *       2) Subtract the input integer zero point.
 *       3) Perform fixed point multiplication.
 *       4) Add the output zero point.
 *       5) Cast to the out_dtype.
 */
Expr RequantizeLower(const Expr& input_tensor, const Expr& input_scale,
                     const Expr& input_zero_point, const Expr& output_scale,
                     const Expr& output_zero_point, const RequantizeAttrs* param,
                     const Array<IndexExpr>& input_shape, const DataType& out_dtype) {
  DataType hp_dtype = DataType::Int(64);

  auto tensor = Cast(input_tensor, hp_dtype);
  // 1) Subtract the input_zero_point
  auto zero_scalar = MakeConstantScalar(DataType::Int(32), 0);
  if (!IsEqualScalar(input_zero_point, zero_scalar)) {
    tensor = Subtract(tensor, Cast(input_zero_point, hp_dtype));
  }

  // 2) If the input and output scales are same, we can skip the fixed point multiplication. Check
  // if the input scale is per-tensor or per-channel. If it is per-tensor, there is single scale for
  // the whole tensor. For per-channel (aka per-axis), there is a vector of scales for the input
  // tensor. Depending on the quantization type, the fixed point multiplication routing is called.
  auto scaled_int64_t = tensor;
  float output_scale_float = GetScalarFromConstant<float>(output_scale);
  if (IsConstScalar(input_scale)) {
    // This is per-tensor quantization. Single scale.
    float input_scale_float = GetScalarFromConstant<float>(input_scale);
    double double_multiplier =
        static_cast<double>(input_scale_float) / static_cast<double>(output_scale_float);
    // Skip if input and output scales are same.
    if (!IsEqualScalar(input_scale, output_scale)) {
      scaled_int64_t =
          FixedPointMultiply(scaled_int64_t, double_multiplier, input_shape, param->rounding);
    }
  } else {
    // This is per-channel (per=axis) quantization.
    std::vector<double> double_multipliers;
    auto input_axis_scales = GetFloatVectorFromConstant(input_scale);
    for (auto input_axis_scale : input_axis_scales) {
      double_multipliers.push_back(static_cast<double>(input_axis_scale) /
                                   static_cast<double>(output_scale_float));
    }
    int axis = param->axis;
    axis = (axis == -1) ? input_shape.size() - 1 : axis;
    scaled_int64_t = FixedPointMultiplyPerChannel(scaled_int64_t, double_multipliers, input_shape,
                                                  axis, param->rounding);
  }

  // 3) Add the output zero point.
  auto shifted_int64_t = scaled_int64_t;
  if (!IsEqualScalar(output_zero_point, zero_scalar)) {
    shifted_int64_t = Add(Cast(output_zero_point, hp_dtype), scaled_int64_t);
  }

  // 4) Clip to the out_dtype min/max.
  auto q_min = GetQmin(out_dtype);
  auto q_max = GetQmax(out_dtype);
  auto clipped_t = Clip(shifted_int64_t, q_min, q_max);
  return Cast(clipped_t, out_dtype);
}

/*
 * \brief Forward rewrite the requantize op.
 * \param ref_call The original call that will be lowered.
 * \param new_args The new mutated args to the call node.
 * \param ctx The node context.
 * \return The sequence of Relay ops for requantize op.
 * \note Lowering of the requantize operation. The requantize operator converts
 *       one quantized tensor to another quantized tensor. For the output
 *       tensor, we are provided with output scale and zero point. The
 *       computation looks like this
 *
 * Q_output = zp_output +  (scale_input)/(scale_ouptut) * (Q_input - zp_input)
 */
Expr RequantizeQnnCanonicalize(const Attrs& attrs, const Array<Expr>& new_args,
                               const Array<tvm::relay::Type>& types) {
  CHECK_EQ(new_args.size(), 5);
  auto& quantized_data = new_args[0];
  auto& input_scale = new_args[1];
  auto& input_zero_point = new_args[2];
  auto& output_scale = new_args[3];
  auto& output_zero_point = new_args[4];
  const auto* param = attrs.as<RequantizeAttrs>();
  CHECK(param != nullptr);

  // Find input shape.
  CHECK_EQ(types.size(), 6);
  auto in_type = types[0];
  auto in_tensor_type = in_type.as<TensorTypeNode>();
  CHECK(in_tensor_type != nullptr) << "Type information missing."
                                   << " Please run infer_type pass.";
  Array<IndexExpr> input_shape = in_tensor_type->shape;

  // Find the output dtype.
  auto out_type = types[5];
  auto out_tensor_type = out_type.as<TensorTypeNode>();
  CHECK(out_tensor_type != nullptr) << "Type information missing."
                                    << " Please run infer_type pass.";
  auto out_dtype = out_tensor_type->dtype;

  // Check rounding validity.
  CHECK(param->rounding == "UPWARD" || param->rounding == "TONEAREST")
      << "QNN requantize supports two rounding modes - UPWARD and "
      << "TONEAREST";
  return RequantizeLower(quantized_data, input_scale, input_zero_point, output_scale,
                         output_zero_point, param, input_shape, out_dtype);
}

/*
 * \brief Infer shape function of Requantize op.
 * \param types The types of input args.
 * \param num_inputs The number of inputs.
 * \param attrs The op attributes.
 * \param reporter The type reporter that sets the dtype and shapes.
 * \return True if the infer shape succeeded.
 */
bool RequantizeRel(const Array<Type>& types, int num_inputs, const Attrs& attrs,
                   const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 6);
  const auto* data = types[0].as<TensorTypeNode>();
  CHECK(data != nullptr);
  const auto in_dtype = data->dtype;
  CHECK(in_dtype == DataType::Int(8) ||
        in_dtype == DataType::UInt(8) ||
        in_dtype == DataType::Int(32))
      << "Input type should be one of [int8, uint8, int32] but was " << in_dtype;

  const RequantizeAttrs* requantize_attrs = attrs.as<RequantizeAttrs>();
  int axis = requantize_attrs->axis;
  axis = (axis == -1) ? data->shape.size() - 1: axis;
  CHECK_LT(axis, static_cast<int>(data->shape.size()))
      << "axis " << requantize_attrs->axis << " is out of range";
  CHECK_GE(axis, 0)
      << "axis " << requantize_attrs->axis << " is out of range";

  // Check and assign types for scale and zero points.
  AssignType(types[1], DataType::Float(32), data->shape[axis], reporter);  // input_scale
  AssignType(types[2], DataType::Int(32), data->shape[axis], reporter);    // input_zero_pt
  // For now, requantize output tensor is limited to full tensor uniform quantization.
  CHECK(IsScalarType(types[3], DataType::Float(32)));  // output_scale
  CHECK(IsScalarType(types[4], DataType::Int(32)));    // output_zero_point

  const Array<tvm::PrimExpr> oshape = data->shape;
  // assign output type
  auto out_dtype = requantize_attrs->out_dtype;
  CHECK(out_dtype == DataType::Int(8) ||
        out_dtype == DataType::UInt(8) ||
        out_dtype == DataType::Int(32))
      << "Output type should be one of [int8, uint8, int32] but was " << out_dtype;
  reporter->Assign(types[5], TensorType(oshape, out_dtype));
  return true;
}

// Positional relay function to create qnn requantize operator
// used by frontend FFI.
Expr MakeRequantize(Expr data, Expr input_scale, Expr input_zero_point, Expr output_scale,
                    Expr output_zero_point, int axis, std::string rounding, DataType out_dtype) {
  auto attrs = make_object<RequantizeAttrs>();
  attrs->axis = axis;
  attrs->rounding = std::move(rounding);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("qnn.requantize");
  return CallNode::make(op, {data, input_scale, input_zero_point, output_scale, output_zero_point},
                        Attrs(attrs), {});
}

RELAY_REGISTER_OP("qnn.requantize")
.describe(R"code(Requantize operator.
The requantize operator converts one quantized tensor to another quantized
tensor. For the output tensor, we are provided with output scale and zero
point. The computation looks like this

Q_output = zp_output +  (scale_input)/(scale_output) * (Q_input - zp_input)

)code" TVM_ADD_FILELINE)
.set_attrs_type<RequantizeAttrs>()
.set_num_inputs(5)
.add_argument("data", "Tensor", "The quantized input tensor.")
.add_argument("input_scale", "Tensor", "The quantization scale of the input tensor.")
.add_argument("input_zero_point", "Tensor", "The quantization zero_point of the input tensor.")
.add_argument("output_scale", "Tensor", "The quantization scale of the output tensor.")
.add_argument("output_zero_point", "Tensor", "The quantization zero_point of the output tensor.")
.set_support_level(11)
.add_type_rel("Requantize", RequantizeRel)
.set_attr<FTVMLegalize>("FTVMQnnCanonicalize", RequantizeQnnCanonicalize);

TVM_REGISTER_GLOBAL("relay.qnn.op._make.requantize")
.set_body_typed(MakeRequantize);

}  // namespace qnn
}  // namespace relay
}  // namespace tvm
