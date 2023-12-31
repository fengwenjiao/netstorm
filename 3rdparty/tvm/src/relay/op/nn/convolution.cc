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
 * \file convolution.cc
 * \brief Convolution operators
 */
#include <tvm/tir/data_layout.h>
#include <tvm/tir/ir_pass.h>
#include <tvm/relay/op.h>
#include <tvm/relay/attrs/nn.h>
#include <vector>

#include "../../pass/infer_layout_util.h"
#include "../op_common.h"
#include "convolution.h"

namespace tvm {
namespace relay {


template<typename T>
Array<Array<Layout> > ConvInferCorrectLayout(
    const Attrs& attrs,
    const Array<Layout>& new_in_layouts,
    const Array<Layout>& old_in_layouts,
    const Array<Array<IndexExpr>> &old_in_shapes) {
  const T* params = attrs.as<T>();

  // We always make other operators to fit the layouts of convolution layers
  // So this inference ignores all inputs
  return Array<Array<Layout> >{{params->data_layout, params->kernel_layout},
                               {params->out_layout == "" ?
                                   params->data_layout : params->out_layout}};
}


template <typename T>
Expr MakeConv(Expr data,
              Expr weight,
              Array<IndexExpr> strides,
              Array<IndexExpr> padding,
              Array<IndexExpr> dilation,
              int groups,
              IndexExpr channels,
              Array<IndexExpr> kernel_size,
              std::string data_layout,
              std::string kernel_layout,
              std::string out_layout,
              DataType out_dtype,
              std::string op_name) {
  auto attrs = make_object<T>();
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = std::move(channels);
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get(op_name);
  return CallNode::make(op, {data, weight}, Attrs(attrs), {});
}


// relay.nn.conv1d
TVM_REGISTER_NODE_TYPE(Conv1DAttrs);

TVM_REGISTER_GLOBAL("relay.op.nn._make.conv1d")
.set_body_typed([](Expr data,
                   Expr weight,
                   Array<IndexExpr> strides,
                   Array<IndexExpr> padding,
                   Array<IndexExpr> dilation,
                   int groups,
                   IndexExpr channels,
                   Array<IndexExpr> kernel_size,
                   std::string data_layout,
                   std::string kernel_layout,
                   std::string out_layout,
                   DataType out_dtype) {
  return MakeConv<Conv1DAttrs>(
    data, weight, strides, padding, dilation,
    groups, channels, kernel_size, data_layout,
    kernel_layout, out_layout, out_dtype, "nn.conv1d");
});


RELAY_REGISTER_OP("nn.conv1d")
.describe(R"code(1D convolution layer (e.g. spatial convolution over sequences).

This layer creates a convolution kernel that is convolved
with the layer input to produce a tensor of outputs.

- **data**: This depends on the `layout` parameter. Input is 3D array of shape
            (batch_size, in_channels, width) if `layout` is `NCW`.
- **weight**: (channels, in_channels, kernel_size)
- **out**:  This depends on the `layout` parameter. Output is 3D array of shape
            (batch_size, channels, out_width) if `layout` is `NCW`.

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv1DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(2)
.add_type_rel("Conv1D", Conv1DRel<Conv1DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ConvInferCorrectLayout<Conv1DAttrs>);


// relay.nn.conv2d
TVM_REGISTER_NODE_TYPE(Conv2DAttrs);

TVM_REGISTER_GLOBAL("relay.op.nn._make.conv2d")
.set_body_typed([](Expr data,
                   Expr weight,
                   Array<IndexExpr> strides,
                   Array<IndexExpr> padding,
                   Array<IndexExpr> dilation,
                   int groups,
                   IndexExpr channels,
                   Array<IndexExpr> kernel_size,
                   std::string data_layout,
                   std::string kernel_layout,
                   std::string out_layout,
                   DataType out_dtype) {
  return MakeConv<Conv2DAttrs>(
    data, weight, strides, padding, dilation,
    groups, channels, kernel_size, data_layout,
    kernel_layout, out_layout, out_dtype, "nn.conv2d");
});


RELAY_REGISTER_OP("nn.conv2d")
.describe(R"code(2D convolution layer (e.g. spatial convolution over images).

This layer creates a convolution kernel that is convolved
with the layer input to produce a tensor of outputs.

- **data**: This depends on the `layout` parameter. Input is 4D array of shape
            (batch_size, in_channels, height, width) if `layout` is `NCHW`.
- **weight**: (channels, in_channels, kernel_size[0], kernel_size[1])
- **out**:  This depends on the `layout` parameter. Output is 4D array of shape
            (batch_size, channels, out_height, out_width) if `layout` is `NCHW`.

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(2)
.add_type_rel("Conv2D", Conv2DRel<Conv2DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ConvInferCorrectLayout<Conv2DAttrs>);

// relay.nn.conv3d
TVM_REGISTER_NODE_TYPE(Conv3DAttrs);

TVM_REGISTER_GLOBAL("relay.op.nn._make.conv3d")
.set_body_typed([](Expr data,
                   Expr weight,
                   Array<IndexExpr> strides,
                   Array<IndexExpr> padding,
                   Array<IndexExpr> dilation,
                   int groups,
                   IndexExpr channels,
                   Array<IndexExpr> kernel_size,
                   std::string data_layout,
                   std::string kernel_layout,
                   std::string out_layout,
                   DataType out_dtype) {
  return MakeConv<Conv3DAttrs>(
    data, weight, strides, padding, dilation,
    groups, channels, kernel_size, data_layout,
    kernel_layout, out_layout, out_dtype, "nn.conv3d");
});


RELAY_REGISTER_OP("nn.conv3d")
.describe(R"code(3D convolution layer (e.g. convolution over 3D image data,
like Magnetic Resonance Imaging (MRI) data in medicine).

This layer creates a convolution kernel that is convolved
with the layer input to produce a tensor of outputs.

- **data**: This depends on the `layout` parameter. Input is 5D array of shape
            (batch_size, in_channels, depth, height, width) if `layout` is `NCDHW`.
- **weight**: (channels, in_channels, kernel_size[0], kernel_size[1], kernel_size[2])
- **out**:  This depends on the `layout` parameter. Output is 5D array of shape
            (batch_size, channels, out_depth, out_height, out_width) if `layout` is `NCDHW`.

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv3DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(2)
.add_type_rel("Conv3D", Conv3DRel<Conv3DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ConvInferCorrectLayout<Conv3DAttrs>);

// relay.nn.conv2d_transpose
TVM_REGISTER_NODE_TYPE(Conv2DTransposeAttrs);

bool Conv2DTransposeRel(const Array<Type>& types,
                        int num_inputs,
                        const Attrs& attrs,
                        const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 3);
  const auto* data = types[0].as<TensorTypeNode>();
  const auto* weight = types[1].as<TensorTypeNode>();
  if (data == nullptr) return false;

  static const Layout kNCHW("NCHW");
  static const Layout kOIHW("OIHW");

  const Conv2DTransposeAttrs* param = attrs.as<Conv2DTransposeAttrs>();
  CHECK(param != nullptr);
  const Layout in_layout(param->data_layout);
  const Layout kernel_layout(param->kernel_layout);

  const auto trans_in_layout = BijectiveLayoutNode::make(in_layout, kNCHW);
  CHECK(trans_in_layout.defined())
    << "Conv only support input layouts that are convertible from NCHW."
    << " But got " << in_layout;

  const auto trans_kernel_layout = BijectiveLayoutNode::make(kernel_layout, kOIHW);
  CHECK(trans_kernel_layout.defined())
    << "Conv only support kernel layouts that are convertible from OIHW."
    << " But got "<< kernel_layout;

  Layout out_layout(param->out_layout == "" ? param->data_layout : param->out_layout);
  const auto trans_out_layout = BijectiveLayoutNode::make(out_layout, kNCHW);
  CHECK(trans_out_layout.defined())
    << "Conv only support output layouts that are convertible from NCHW."
    << " But got " << out_layout;

  IndexExpr channels, dilated_ksize_y, dilated_ksize_x;

  auto dshape_nchw = trans_in_layout.ForwardShape(data->shape);

  // infer weight if the kernel_size and channels are defined
  if (param->kernel_size.defined() && param->channels.defined()) {
    CHECK_EQ(param->kernel_size.size(), 2);
    CHECK_EQ(param->dilation.size(), 2);

    Array<IndexExpr> wshape({dshape_nchw[1],
            indexdiv(param->channels, param->groups),
            param->kernel_size[0],
            param->kernel_size[1]});

    wshape = trans_kernel_layout.BackwardShape(wshape);
    dilated_ksize_y = 1 + (param->kernel_size[0] - 1) * param->dilation[0];
    dilated_ksize_x = 1 + (param->kernel_size[1] - 1) * param->dilation[1];
    channels = param->channels;

    // assign result to reporter
    reporter->Assign(types[1], TensorType(wshape, data->dtype));
  } else {
    // use weight to infer the conv shape.
    if (weight == nullptr) return false;
    auto wshape = trans_kernel_layout.ForwardShape(weight->shape);
    if (param->kernel_size.defined()) {
      CHECK_EQ(param->kernel_size.size(), 2);
      // check the size
      CHECK(reporter->AssertEQ(param->kernel_size[0], wshape[2]) &&
            reporter->AssertEQ(param->kernel_size[1], wshape[3]))
          << "Conv2D: shape of weight is inconsistent with kernel_size, "
          << " kernel_size=" << param->kernel_size
          << " wshape=" << Array<IndexExpr>(wshape);
    }
    if (param->channels.defined()) {
      CHECK(reporter->AssertEQ(param->channels, wshape[1]))
          << "Conv2D: shape of weight is inconsistent with channels, "
          << " channels=" << param->channels
          << " wshape=" << Array<IndexExpr>(wshape);
    }
    CHECK(reporter->AssertEQ(indexdiv(dshape_nchw[1], param->groups), wshape[0]));
    channels = wshape[1];
    dilated_ksize_y = 1 + (wshape[2] - 1) * param->dilation[0];
    dilated_ksize_x = 1 + (wshape[3] - 1) * param->dilation[1];
  }
  // dilation
  Array<IndexExpr> oshape({dshape_nchw[0], channels, 0, 0});
  IndexExpr pad_h, pad_w;
  GetPaddingHeightWidth(param->padding, &pad_h, &pad_w);
  oshape.Set(2, (param->strides[0] * (dshape_nchw[2] - 1) + dilated_ksize_y -
                 pad_h + param->output_padding[0]));
  oshape.Set(3, (param->strides[1] * (dshape_nchw[3] - 1) + dilated_ksize_x -
                 pad_w + param->output_padding[1]));

  DataType out_dtype = param->out_dtype;
  if (out_dtype.bits() == 0) {
    out_dtype = data->dtype;
  }
  oshape = trans_out_layout.BackwardShape(oshape);
  reporter->Assign(types[2], TensorType(oshape, out_dtype));
  return true;
}


Expr MakeConv2DTranspose(Expr data,
                         Expr weight,
                         Array<IndexExpr> strides,
                         Array<IndexExpr> padding,
                         Array<IndexExpr> dilation,
                         int groups,
                         IndexExpr channels,
                         Array<IndexExpr> kernel_size,
                         std::string data_layout,
                         std::string kernel_layout,
                         std::string out_layout,
                         Array<IndexExpr> output_padding,
                         DataType out_dtype) {
  auto attrs = make_object<Conv2DTransposeAttrs>();
  attrs->channels = std::move(channels);
  attrs->kernel_size = std::move(kernel_size);
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->output_padding = std::move(output_padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.conv2d_transpose");
  return CallNode::make(op, {data, weight}, Attrs(attrs), {});
}


TVM_REGISTER_GLOBAL("relay.op.nn._make.conv2d_transpose")
.set_body_typed(MakeConv2DTranspose);

RELAY_REGISTER_OP("nn.conv2d_transpose")
.describe(R"code(Transposed 2D convolution layer (sometimes called Deconvolution).

The need for transposed convolutions generally arises
from the desire to use a transformation going in the opposite direction
of a normal convolution, i.e., from something that has the shape of the
output of some convolution to something that has the shape of its input
while maintaining a connectivity pattern that is compatible with
said convolution.

- **data**: This depends on the `layout` parameter. Input is 4D array of shape
            (batch_size, in_channels, height, width) if `layout` is `NCHW`.
- **weight**: (in_channels, channels, kernel_size[0], kernel_size[1])
- **bias**: (channels,)
- **out**:  This depends on the `layout` parameter. Output is 4D array of shape
v            (batch_size, channels, out_height, out_width) if `layout` is `NCHW`.

            out_height and out_width are calculated as::
                out_height = (height-1)*strides[0]-2*padding[0]+kernel_size[0]+output_padding[0]
                out_width = (width-1)*strides[1]-2*padding[1]+kernel_size[1]+output_padding[1]

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DTransposeAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(2)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
                               ConvInferCorrectLayout<Conv2DTransposeAttrs>)
.add_type_rel("Conv2DTranspose", Conv2DTransposeRel);


// relay.nn.conv1d_transpose
TVM_REGISTER_NODE_TYPE(Conv1DTransposeAttrs);

bool Conv1DTransposeRel(const Array<Type>& types,
                        int num_inputs,
                        const Attrs& attrs,
                        const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 3);
  const auto* data = types[0].as<TensorTypeNode>();
  const auto* weight = types[1].as<TensorTypeNode>();
  if (data == nullptr) return false;

  static const Layout kNCW("NCW");
  static const Layout kOIW("OIW");

  const Conv1DTransposeAttrs* param = attrs.as<Conv1DTransposeAttrs>();
  CHECK(param != nullptr);
  const Layout in_layout(param->data_layout);
  const Layout kernel_layout(param->kernel_layout);

  const auto trans_in_layout = BijectiveLayoutNode::make(in_layout, kNCW);
  CHECK(trans_in_layout.defined())
    << "Conv only support input layouts that are convertible from NCW."
    << " But got " << in_layout;

  const auto trans_kernel_layout = BijectiveLayoutNode::make(kernel_layout, kOIW);
  CHECK(trans_kernel_layout.defined())
    << "Conv only support kernel layouts that are convertible from OIW."
    << " But got "<< kernel_layout;

  Layout out_layout(param->out_layout == "" ? param->data_layout : param->out_layout);
  const auto trans_out_layout = BijectiveLayoutNode::make(out_layout, kNCW);
  CHECK(trans_out_layout.defined())
    << "Conv only support output layouts that are convertible from NCW."
    << " But got " << out_layout;

  IndexExpr channels, dilated_ksize_y, dilated_ksize_x;

  auto dshape_ncw = trans_in_layout.ForwardShape(data->shape);

  // infer weight if the kernel_size and channels are defined
  if (param->kernel_size.defined() && param->channels.defined()) {
    CHECK_EQ(param->kernel_size.size(), 1);
    CHECK_EQ(param->dilation.size(), 1);

    Array<IndexExpr> wshape({dshape_ncw[1],
            indexdiv(param->channels, param->groups),
            param->kernel_size[0]});

    wshape = trans_kernel_layout.BackwardShape(wshape);
    dilated_ksize_x = 1 + (param->kernel_size[0] - 1) * param->dilation[0];
    channels = param->channels;

    // assign result to reporter
    reporter->Assign(types[1], TensorType(wshape, data->dtype));
  } else {
    // use weight to infer the conv shape.
    if (weight == nullptr) return false;
    auto wshape = trans_kernel_layout.ForwardShape(weight->shape);
    if (param->kernel_size.defined()) {
      CHECK_EQ(param->kernel_size.size(), 1);
      // check the size
      CHECK(reporter->AssertEQ(param->kernel_size[0], wshape[2]))
          << "Conv1D: shape of weight is inconsistent with kernel_size, "
          << " kernel_size=" << param->kernel_size
          << " wshape=" << Array<IndexExpr>(wshape);
    }
    if (param->channels.defined()) {
      CHECK(reporter->AssertEQ(param->channels, wshape[1]))
          << "Conv1D: shape of weight is inconsistent with channels, "
          << " channels=" << param->channels
          << " wshape=" << Array<IndexExpr>(wshape);
    }
    CHECK(reporter->AssertEQ(indexdiv(dshape_ncw[1], param->groups), wshape[0]));
    channels = wshape[1];
    dilated_ksize_x = 1 + (wshape[2] - 1) * param->dilation[0];
  }
  // dilation
  IndexExpr pad_w;
  GetPaddingWidth(param->padding, &pad_w);
  Array<IndexExpr> oshape({dshape_ncw[0], channels, 0});
  oshape.Set(2, (param->strides[0] * (dshape_ncw[2] - 1) + dilated_ksize_x -
                 pad_w + param->output_padding[0]));

  DataType out_dtype = param->out_dtype;
  if (out_dtype.bits() == 0) {
    out_dtype = data->dtype;
  }
  oshape = trans_out_layout.BackwardShape(oshape);
  reporter->Assign(types[2], TensorType(oshape, out_dtype));
  return true;
}


Expr MakeConv1DTranspose(Expr data,
                         Expr weight,
                         Array<IndexExpr> strides,
                         Array<IndexExpr> padding,
                         Array<IndexExpr> dilation,
                         int groups,
                         IndexExpr channels,
                         Array<IndexExpr> kernel_size,
                         std::string data_layout,
                         std::string kernel_layout,
                         std::string out_layout,
                         Array<IndexExpr> output_padding,
                         DataType out_dtype) {
  auto attrs = make_object<Conv1DTransposeAttrs>();
  attrs->channels = std::move(channels);
  attrs->kernel_size = std::move(kernel_size);
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->output_padding = std::move(output_padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.conv1d_transpose");
  return CallNode::make(op, {data, weight}, Attrs(attrs), {});
}


TVM_REGISTER_GLOBAL("relay.op.nn._make.conv1d_transpose")
.set_body_typed(MakeConv1DTranspose);

RELAY_REGISTER_OP("nn.conv1d_transpose")
.describe(R"code(Transposed 1D convolution layer (sometimes called Deconvolution).

The need for transposed convolutions generally arises
from the desire to use a transformation going in the opposite direction
of a normal convolution, i.e., from something that has the shape of the
output of some convolution to something that has the shape of its input
while maintaining a connectivity pattern that is compatible with
said convolution.

- **data**: This depends on the `layout` parameter. Input is 3D array of shape
            (batch_size, in_channels, width) if `layout` is `NCW`.
- **weight**: (in_channels, channels, kernel_size[0])
- **bias**: (channels,)
- **out**:  This depends on the `layout` parameter. Output is 3D array of shape
            (batch_size, channels, out_width) if `layout` is `NCW`.

            out_width is calculated as::
                out_width = (width-1)*strides[0]-2*padding[0]+kernel_size[0]+output_padding[0]

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv1DTransposeAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(2)
.add_type_rel("Conv1DTranspose", Conv1DTransposeRel);


// relay.nn.contrib_conv2d_winograd_without_weight_transform
TVM_REGISTER_NODE_TYPE(Conv2DWinogradAttrs);

template<class Param>
bool Conv2DWinogradRel(const Array<Type>& types,
                       int num_inputs,
                       const Attrs& attrs,
                       const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 3);
  const auto* data = types[0].as<TensorTypeNode>();
  if (data == nullptr) return false;
  static const Layout kNCHW("NCHW");
  static const Layout kOIHW("OIHW");

  const Param* param = attrs.as<Param>();
  CHECK(param != nullptr);
  const Layout in_layout(param->data_layout);
  const Layout kernel_layout(param->kernel_layout);

  const auto trans_in_layout = BijectiveLayoutNode::make(in_layout, kNCHW);
  CHECK(trans_in_layout.defined())
    << "Conv only support input layouts that are convertible from NCHW."
    << " But got " << in_layout;

  const auto trans_kernel_layout = BijectiveLayoutNode::make(kernel_layout, kOIHW);
  CHECK(trans_kernel_layout.defined())
    << "Conv only support kernel layouts that are convertible from OIHW."
    << " But got "<< kernel_layout;

  Layout out_layout(param->out_layout == "" ? param->data_layout : param->out_layout);
  const auto trans_out_layout = BijectiveLayoutNode::make(out_layout, kNCHW);
  CHECK(trans_out_layout.defined())
      << "Conv only support output layouts that are convertible from NCHW."
      << " But got " << out_layout;

  Array<IndexExpr> dshape_nchw = trans_in_layout.ForwardShape(data->shape);

  IndexExpr channels, dilated_ksize_y, dilated_ksize_x;

  CHECK(param->kernel_size.defined() && param->channels.defined())
      << "The kernel size and channels of a Conv must be set or infered by previous pass";

  CHECK_EQ(param->kernel_size.size(), 2);
  CHECK_EQ(param->dilation.size(), 2);

  channels = param->channels;
  dilated_ksize_y = 1 + (param->kernel_size[0] - 1) * param->dilation[0];
  dilated_ksize_x = 1 + (param->kernel_size[1] - 1) * param->dilation[1];

  // NOTE: Do not check weight shape here!
  // Different backend requires different layout to compute
  // the batch gemm stage in winograd efficiently, but we want to
  // make this op work for all backends.
  // So we accept all weight shapes, and assume the TOPI developers
  // can handle this correctly in alter_op_layout.

  // dilation
  Array<IndexExpr> oshape({dshape_nchw[0], channels, 0, 0});

  IndexExpr pad_h, pad_w;
  GetPaddingHeightWidth(param->padding, &pad_h, &pad_w);
  if (!dshape_nchw[2].as<tir::AnyNode>()) {
    oshape.Set(2, (dshape_nchw[2] + pad_h
                   - dilated_ksize_y) / param->strides[0] + 1);
  } else {
    oshape.Set(2, dshape_nchw[2]);
  }
  if (!dshape_nchw[3].as<tir::AnyNode>()) {
    oshape.Set(3, (dshape_nchw[3] + pad_w
                   - dilated_ksize_x) / param->strides[1] + 1);
  } else {
    oshape.Set(3, dshape_nchw[3]);
  }

  DataType out_dtype = param->out_dtype;
  if (out_dtype.bits() == 0) {
    out_dtype = data->dtype;
  }
  oshape = trans_out_layout.BackwardShape(oshape);
  // assign output type
  reporter->Assign(types[2], TensorType(oshape, out_dtype));
  return true;
}


// Positional relay function to create conv2d winograd operator
// used by frontend FFI.
Expr MakeConv2DWinograd(Expr data,
                        Expr weight,
                        int tile_size,
                        Array<IndexExpr> strides,
                        Array<IndexExpr> padding,
                        Array<IndexExpr> dilation,
                        int groups,
                        IndexExpr channels,
                        Array<IndexExpr> kernel_size,
                        std::string data_layout,
                        std::string kernel_layout,
                        std::string out_layout,
                        DataType out_dtype) {
  auto attrs = make_object<Conv2DWinogradAttrs>();
  attrs->tile_size = tile_size;
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_conv2d_winograd_without_weight_transform");
  return CallNode::make(op, {data, weight}, Attrs(attrs), {});
}


TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_winograd_without_weight_transform")
.set_body_typed(MakeConv2DWinograd);


RELAY_REGISTER_OP("nn.contrib_conv2d_winograd_without_weight_transform")
.describe(R"code(Compute conv2d with winograd algorithm. Only supports NCHW layout.
                 This operator assumes the weight tensor is already pre-transformed by
                 nn.contrib_conv2d_winograd_weight_transform.

- **data**: Input is 4D array of shape  (batch_size, in_channels, height, width)
- **weight**: Any shape
            We do not check the shape for this input tensor. Since different backend
            has different layout strategy.

- **out**:  Output is 4D array of shape (batch_size, channels, out_height, out_width)
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DWinogradAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DWinograd", Conv2DWinogradRel<Conv2DWinogradAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
        ConvInferCorrectLayout<Conv2DWinogradAttrs>);

// relay.nn.contrib_conv2d_winograd_weight_transform
TVM_REGISTER_NODE_TYPE(Conv2DWinogradWeightTransformAttrs);

bool Conv2DWinogradWeightTransformRel(const Array<Type>& types,
                                      int num_inputs,
                                      const Attrs& attrs,
                                      const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 2);
  const auto* data = types[0].as<TensorTypeNode>();
  if (data == nullptr) return false;

  const Conv2DWinogradWeightTransformAttrs* param = attrs.as<Conv2DWinogradWeightTransformAttrs>();
  CHECK(param != nullptr);

  CHECK_EQ(data->shape.size(), 4) << "Only support NCHW normal kernel layout";

  // each pad width element should be a pair of positive integers
  std::vector<IndexExpr> oshape {
      param->tile_size + data->shape[2] - 1,
      param->tile_size + data->shape[3] - 1,
      data->shape[0],
      data->shape[1],
  };

  reporter->Assign(types[1], TensorType(Array<IndexExpr>(oshape),
                                                  data->dtype));
  return true;
}

Expr MakeConv2DWinogradWeightTransform(Expr weight,
                                       int tile_size) {
  auto attrs = make_object<Conv2DWinogradWeightTransformAttrs>();
  attrs->tile_size = tile_size;
  static const Op& op = Op::Get("nn.contrib_conv2d_winograd_weight_transform");
  return CallNode::make(op, {weight}, Attrs(attrs), {});
}


TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_winograd_weight_transform")
.set_body_typed(MakeConv2DWinogradWeightTransform);


RELAY_REGISTER_OP("nn.contrib_conv2d_winograd_weight_transform")
.describe(R"code(Weight transformation of winograd fast convolution algorithm.

Separate this into another operator in order to enable Precompute Pass to compute the
weight transformation in advance.

- **weight**: (channels, in_channels, kernel_size[0], kernel_size[1])
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DWinogradWeightTransformAttrs>()
.set_num_inputs(1)
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DWinogradWeightTransform", Conv2DWinogradWeightTransformRel);


// Positional relay function to create conv2d winograd nnpack operator
// used by frontend FFI.
Expr MakeConv2DWinogradNNPACK(Expr data,
                              Expr weight,
                              Array<IndexExpr> strides,
                              Array<IndexExpr> padding,
                              Array<IndexExpr> dilation,
                              int groups,
                              IndexExpr channels,
                              Array<IndexExpr> kernel_size,
                              std::string data_layout,
                              std::string kernel_layout,
                              std::string out_layout,
                              DataType out_dtype) {
  auto attrs = make_object<Conv2DAttrs>();
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_conv2d_winograd_nnpack_without_weight_transform");
  return CallNode::make(op, {data, weight}, Attrs(attrs), {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_winograd_nnpack_without_weight_transform")
.set_body_typed(MakeConv2DWinogradNNPACK);

RELAY_REGISTER_OP("nn.contrib_conv2d_winograd_nnpack_without_weight_transform")
.describe(R"code(Compute conv2d with winograd nnpack. Only supports NCHW layout.
              This operator assumes the weight tensor is already pre-transformed by
              nn.contrib_conv2d_winograd_nnpack_weight_transform.

- **data**: Input is 4D array of shape  (batch_size, in_channels, height, width)
- **weight**: Any shape
            We do not check the shape for this input tensor. Since different backend
            has different layout strategy.

- **out**:  Output is 4D array of shape (batch_size, channels, out_height, out_width)
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DWinogradNNPACKRel", Conv2DWinogradRel<Conv2DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout", ConvInferCorrectLayout<Conv2DAttrs>);

// relay.nn.contrib_conv2d_winograd_nnpack_weight_transform
TVM_REGISTER_NODE_TYPE(Conv2DWinogradNNPACKWeightTransformAttrs);

bool Conv2DWinogradNNPACKWeightTransformRel(const Array<Type>& types,
                                            int num_inputs,
                                            const Attrs& attrs,
                                            const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 2);
  const auto* data = types[0].as<TensorTypeNode>();
  if (data == nullptr) {
    return false;
  }

  const Conv2DWinogradNNPACKWeightTransformAttrs* param =
      attrs.as<Conv2DWinogradNNPACKWeightTransformAttrs>();
  CHECK(param != nullptr);

  CHECK_EQ(data->shape.size(), 4) << "Only support NCHW normal kernel layout";

  std::vector<IndexExpr> oshape{
      data->shape[0],
      data->shape[1],
      8,
      8,
  };

  DataType out_dtype = param->out_dtype;
  if (out_dtype.bits() == 0) {
    out_dtype = data->dtype;
  }
  reporter->Assign(types[1], TensorType(Array<IndexExpr>(oshape), out_dtype));
  return true;
}

Expr MakeConv2DWinogradNNPACKWeightTransform(Expr weight,
                                             int convolution_algorithm,
                                             DataType out_dtype) {
  auto attrs = make_object<Conv2DWinogradNNPACKWeightTransformAttrs>();
  attrs->convolution_algorithm = convolution_algorithm;
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_conv2d_winograd_nnpack_weight_transform");
  return CallNode::make(op, {weight}, Attrs(attrs), {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_winograd_nnpack_weight_transform")
.set_body_typed(MakeConv2DWinogradNNPACKWeightTransform);

RELAY_REGISTER_OP("nn.contrib_conv2d_winograd_nnpack_weight_transform")
.describe(R"code(Weight transformation of winograd fast convolution algorithm with NNPACK.
Separate this into another symbol in order to enable Precompute Pass to compute the
weight transformation in advance.

- **weight**: (channels, in_channels, kernel_size[0], kernel_size[1])

)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DWinogradNNPACKWeightTransformAttrs>()
.set_num_inputs(1)
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DWinogradNNPACKWeightTransform", Conv2DWinogradNNPACKWeightTransformRel);

// Positional relay function to create conv2d NCHWc operator
// used by frontend FFI.
Expr MakeConv2DNCHWcInt8(Expr data,
                         Expr kernel,
                         Array<IndexExpr> strides,
                         Array<IndexExpr> padding,
                         Array<IndexExpr> dilation,
                         int groups,
                         IndexExpr channels,
                         Array<IndexExpr> kernel_size,
                         std::string data_layout,
                         std::string kernel_layout,
                         std::string out_layout,
                         DataType out_dtype) {
  auto attrs = make_object<Conv2DAttrs>();
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_conv2d_NCHWc_int8");
  return CallNode::make(op, {data, kernel}, Attrs(attrs), {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_NCHWc_int8")
.set_body_typed(MakeConv2DNCHWcInt8);


RELAY_REGISTER_OP("nn.contrib_conv2d_NCHWc_int8")
.describe(R"code(Compute conv2d with NCHWc data layout with int8 inputs.
- **data**: Input is 5D packed tensor.
- **weight**: 7D packed tensor.

- **out**:  Output is 5D packed tensor
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DNCHWcInt8", Conv2DWinogradRel<Conv2DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
        ConvInferCorrectLayout<Conv2DAttrs>);

// Positional relay function to create conv2d NCHWc operator
// used by frontend FFI.
Expr MakeConv2DNCHWc(Expr data,
                     Expr kernel,
                     Array<IndexExpr> strides,
                     Array<IndexExpr> padding,
                     Array<IndexExpr> dilation,
                     int groups,
                     IndexExpr channels,
                     Array<IndexExpr> kernel_size,
                     std::string data_layout,
                     std::string kernel_layout,
                     std::string out_layout,
                     DataType out_dtype) {
  auto attrs = make_object<Conv2DAttrs>();
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_conv2d_NCHWc");
  return CallNode::make(op, {data, kernel}, Attrs(attrs), {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_conv2d_NCHWc")
.set_body_typed(MakeConv2DNCHWc);


RELAY_REGISTER_OP("nn.contrib_conv2d_NCHWc")
.describe(R"code(Compute conv2d with NCHWc data layout. Only supports NCHW layout.
- **data**: Input is 5D packed tensor.
- **weight**: 6D packed tensor.

- **out**:  Output is 5D packed tensor
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2DNCHWc", Conv2DWinogradRel<Conv2DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
        ConvInferCorrectLayout<Conv2DAttrs>);


// Positional relay function to create depthwise conv2d NCHWc operator
// used by frontend FFI.
Expr MakeDepthwiseConv2DNCHWc(Expr data,
                              Expr kernel,
                              Array<IndexExpr> strides,
                              Array<IndexExpr> padding,
                              Array<IndexExpr> dilation,
                              int groups,
                              IndexExpr channels,
                              Array<IndexExpr> kernel_size,
                              std::string data_layout,
                              std::string kernel_layout,
                              std::string out_layout,
                              DataType out_dtype) {
  auto attrs = make_object<Conv2DAttrs>();
  attrs->strides = std::move(strides);
  attrs->padding = std::move(padding);
  attrs->dilation = std::move(dilation);
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = std::move(kernel_size);
  attrs->data_layout = std::move(data_layout);
  attrs->kernel_layout = std::move(kernel_layout);
  attrs->out_layout = std::move(out_layout);
  attrs->out_dtype = std::move(out_dtype);
  static const Op& op = Op::Get("nn.contrib_depthwise_conv2d_NCHWc");
  return CallNode::make(op, {data, kernel}, Attrs(attrs), {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.contrib_depthwise_conv2d_NCHWc")
.set_body_typed(MakeDepthwiseConv2DNCHWc);


RELAY_REGISTER_OP("nn.contrib_depthwise_conv2d_NCHWc")
.describe(R"code(Compute conv2d with NCHWc data layout. Only supports NCHW layout.
- **data**: Input is 5D packed tensor.
- **weight**: 6D packed tensor.

- **out**:  Output is 5D packed tensor
)code" TVM_ADD_FILELINE)
.set_attrs_type<Conv2DAttrs>()
.set_num_inputs(2)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(10)
.add_type_rel("Conv2D", Conv2DRel<Conv2DAttrs>)
.set_attr<FInferCorrectLayout>("FInferCorrectLayout",
        ConvInferCorrectLayout<Conv2DAttrs>);


bool DeformableConv2DRel(const Array<Type>& types, int num_inputs, const Attrs& attrs,
                         const TypeReporter& reporter) {
  CHECK_EQ(types.size(), 4);
  const auto* data = types[0].as<TensorTypeNode>();
  const auto* weight = types[2].as<TensorTypeNode>();

  CHECK(data);
  auto* param = attrs.as<DeformableConv2DAttrs>();
  CHECK_EQ(param->data_layout, "NCHW") << "data layout not supported.";
  CHECK_EQ(param->kernel_layout, "OIHW") << "kernel_layout not supported.";

  IndexExpr channels, dilated_ksize_y, dilated_ksize_x, ksize_y, ksize_x;

  // infer weight shape if kernel_size and channels are defiend
  if (param->kernel_size.defined() && param->channels.defined()) {
    CHECK_EQ(param->kernel_size.size(), 2);
    CHECK_EQ(param->dilation.size(), 2);
    Array<IndexExpr> wshape(
       {param->channels,
         indexdiv(data->shape[1], param->groups),
         param->kernel_size[0],
         param->kernel_size[1]});
    channels = param->channels;
    ksize_y = param->kernel_size[0];
    ksize_x = param->kernel_size[1];
    dilated_ksize_y = 1 + (param->kernel_size[0] - 1) * param->dilation[0];
    dilated_ksize_x = 1 + (param->kernel_size[1] - 1) * param->dilation[1];
    // assign result to reporter
    reporter->Assign(types[2], TensorType(wshape, data->dtype));
  } else {
    // use weight to infer the conv shape.
    if (weight == nullptr) return false;
    auto wshape = weight->shape;
    if (param->kernel_size.defined()) {
      CHECK_EQ(param->kernel_size.size(), 2);
      // check the size
      CHECK(reporter->AssertEQ(param->kernel_size[0], wshape[2]) &&
            reporter->AssertEQ(param->kernel_size[1], wshape[3]))
          << "DeformableConv2D: shape of weight is inconsistent with kernel_size, "
          << " kernel_size=" << param->kernel_size
          << " wshape=" << wshape;
    }
    if (param->channels.defined()) {
      CHECK(reporter->AssertEQ(param->channels, wshape[0]))
          << "DeformableConv2D: shape of weight is inconsistent with channels, "
          << " channels=" << param->channels
          << " wshape=" << wshape;
    }
    CHECK(reporter->AssertEQ(indexdiv(data->shape[1], param->groups), wshape[1]));
    channels = wshape[0];
    ksize_y = wshape[2];
    ksize_x = wshape[3];
    dilated_ksize_y = 1 + (wshape[2] - 1) * param->dilation[0];
    dilated_ksize_x = 1 + (wshape[3] - 1) * param->dilation[1];
  }
  // dilation
  Array<IndexExpr> oshape({data->shape[0], channels, 0, 0});

  IndexExpr pad_h, pad_w;
  GetPaddingHeightWidth(param->padding, &pad_h, &pad_w);
  oshape.Set(2, indexdiv(data->shape[2] + pad_h - dilated_ksize_y,
                         param->strides[0]) + 1);
  oshape.Set(3, indexdiv(data->shape[3] + pad_w - dilated_ksize_x,
                         param->strides[1]) + 1);
  DataType out_dtype = param->out_dtype;

  // infer offset shape
  Array<IndexExpr> offset_shape({data->shape[0], 2 * ksize_y * ksize_x * param->deformable_groups,
          oshape[2], oshape[3]});
  reporter->Assign(types[1], TensorType(offset_shape, data->dtype));
  if (out_dtype.bits() == 0) {
    out_dtype = data->dtype;
  }

  reporter->Assign(types[3], TensorType(oshape, out_dtype));
  return true;
}


TVM_REGISTER_NODE_TYPE(DeformableConv2DAttrs);

RELAY_REGISTER_OP("nn.deformable_conv2d")
    .describe(R"code(Compute 2-D deformable convolution on 4-D input.
The deformable convolution operation is described in https://arxiv.org/abs/1703.06211

For 2-D deformable convolution, the shapes are
- **data**: (batch_size, channel, height, width)
- **offset**: (batch_size, deformable_groups * kernel[0] * kernel[1] * 2, out_height, out_width)
- **weight**: (num_filter, channel, kernel[0], kernel[1])
- **out**: (batch_size, num_filter, out_height, out_width).

If `deformable_groups` is larger than 1, denoted by *dg*, then split the
input `offset` evenly into *dg* parts along the channel axis, and also evenly split `out`
evenly into *dg* parts along the channel axis. Next compute the deformable convolution, apply the
*i*-th part of the offset part on the *i*-th out.

If `groups` is larger than 1, denoted by *g*, then split the input `data` evenly into *g* parts
along the channel axis, and also evenly split `weight` along the first dimension. Next compute
the convolution on the *i*-th part of the data with the *i*-th weight part. The output is obtained
by concating all the *g* results.
)code" TVM_ADD_FILELINE)
.set_attrs_type<DeformableConv2DAttrs>()
.set_num_inputs(3)
.add_argument("data", "Tensor", "The input tensor.")
.add_argument("offset", "Tensor", "The offset tensor.")
.add_argument("weight", "Tensor", "The weight tensor.")
.set_support_level(5)
.add_type_rel("DeformableConv2D", DeformableConv2DRel);

// Positional relay function to create deformable_conv2d operator
// used by frontend FFI.
Expr MakeDeformableConv2D(Expr data,
                          Expr offset,
                          Expr weight,
                          Array<IndexExpr> strides,
                          Array<IndexExpr> padding,
                          Array<IndexExpr> dilation,
                          int deformable_groups,
                          int groups,
                          int channels,
                          Array<IndexExpr> kernel_size,
                          std::string data_layout,
                          std::string kernel_layout,
                          std::string out_layout,
                          DataType out_dtype) {
  auto attrs = make_object<DeformableConv2DAttrs>();
  attrs->strides = strides;
  attrs->padding = padding;
  attrs->dilation = dilation;
  attrs->deformable_groups = deformable_groups;
  attrs->groups = groups;
  attrs->channels = channels;
  attrs->kernel_size = kernel_size;
  attrs->data_layout = data_layout;
  attrs->kernel_layout = kernel_layout;
  attrs->out_layout = out_layout;
  attrs->out_dtype = out_dtype;
  static const Op& op = Op::Get("nn.deformable_conv2d");
  return CallNode::make(op, {data, offset, weight}, Attrs{attrs}, {});
}

TVM_REGISTER_GLOBAL("relay.op.nn._make.deformable_conv2d")
.set_body_typed(MakeDeformableConv2D);


}  // namespace relay
}  // namespace tvm
