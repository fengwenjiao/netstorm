# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
# pylint: disable=invalid-name,unused-variable,unused-argument,no-member
"""Conv2D int8 schedule on ARM"""

import tvm
from tvm import autotvm
from .. import generic, tag
from ..util import get_const_tuple
from ..nn.conv2d import conv2d_NCHWc_int8
from ..generic import conv2d as conv2d_generic
from .. import nn
from ..nn.conv2d import _get_workload as _get_conv2d_workload
from .tensor_intrin import dot_int8_int8_int32


def _get_default_config(cfg, data, kernel, strides, padding, out_dtype):
    """
    Get default int8 schedule config for the workload
    """
    wkl = _get_conv2d_workload(data, kernel, strides, padding, out_dtype)
    is_kernel_1x1 = wkl.hkernel == 1 and wkl.wkernel == 1
    if is_kernel_1x1:
        conv2d_generic.fallback_schedule_cpu_1x1_int8(
            cfg, wkl, int32_lanes=2, num_int8_elements=4)
    else:
        conv2d_generic.fallback_schedule_cpu_common_int8(
            cfg, wkl, int32_lanes=2, num_int8_elements=4)


@autotvm.register_topi_compute(conv2d_NCHWc_int8, ['arm_cpu'], 'direct')
def _declaration_conv_NCHWc_int8(cfg, data, kernel, strides,
                                 padding, dilation, layout, out_layout, out_dtype):
    # layout and out_layout are not used here,
    # we keep them for debug convenience when dumping autotvm workload
    n, ic_chunk, ih, iw, ic_bn = get_const_tuple(data.shape)
    in_channel = ic_chunk * ic_bn

    oc_chunk, ic_chunk, kh, kw, ic_bn, oc_bn, n_elems = get_const_tuple(kernel.shape)
    num_filter = oc_chunk * oc_bn

    # If no config was set, we can fallback to NCHW config.
    if cfg.is_fallback:
        _get_default_config(cfg, tvm.placeholder((n, in_channel, ih, iw), dtype=data.dtype),
                            tvm.placeholder((num_filter, in_channel, kh, kw), dtype=kernel.dtype),
                            strides, padding, out_dtype)
    return nn.conv2d_NCHWc_int8_compute(data,
                                        kernel,
                                        strides,
                                        padding,
                                        dilation,
                                        layout,
                                        out_layout,
                                        out_dtype)


@autotvm.register_topi_schedule(generic.schedule_conv2d_NCHWc_int8, ['arm_cpu'], ['direct'])
def _schedule_conv2d_NCHWc_int8(cfg, outs):
    """Create schedule for tensors"""
    s = tvm.create_schedule([x.op for x in outs])
    scheduled_ops = []

    def traverse(op):
        """Traverse operators from computation graph"""
        # inline all one-to-one-mapping operators except the last stage (output)
        if tag.is_broadcast(op.tag):
            if op not in s.outputs:
                s[op].compute_inline()
            for tensor in op.input_tensors:
                if isinstance(tensor.op, tvm.tensor.ComputeOp) and tensor.op not in scheduled_ops:
                    traverse(tensor.op)

        if 'conv2d_NCHWc_int8' in op.tag:
            conv_out = op.output(0)
            kernel = conv_out.op.input_tensors[1]
            data_vec = conv_out.op.input_tensors[0]
            data = data_vec.op.input_tensors[0] \
                if isinstance(data_vec.op, tvm.tensor.ComputeOp) and "pad" not in data_vec.op.tag \
                else data_vec
            if isinstance(data.op, tvm.tensor.ComputeOp) and "pad" in data.op.tag:
                data_pad = data
                data = data_pad.op.input_tensors[0]

            args = [s, cfg, data_vec, conv_out, outs[0]]
            # int8 conv kernel is 7-dim
            _, _, kh, kw, _, _, _ = get_const_tuple(kernel.shape)
            dtype = "uint" if data.dtype == "uint8" else "int"
            if kh == 1 and kw == 1:
                conv2d_generic.schedule_conv_NCHWc_cpu_1x1_int8(
                    *args, int32_lanes=4, intrin=dot_int8_int8_int32(int32_lanes=4, dtype=dtype))
            else:
                conv2d_generic.schedule_conv_NCHWc_cpu_common_int8(
                    *args, int32_lanes=4, intrin=dot_int8_int8_int32(int32_lanes=4, dtype=dtype))

        scheduled_ops.append(op)

    traverse(outs[0].op)
    return s
