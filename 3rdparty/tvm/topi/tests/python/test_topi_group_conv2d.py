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
"""Example code to do group convolution."""

import numpy as np
import tvm
from tvm import autotvm
from tvm.autotvm.task.space import FallbackConfigEntity
import topi
import topi.testing
from tvm.contrib.pickle_memoize import memoize
from topi.util import get_const_tuple

from common import get_all_backend, Int8Fallback


def verify_group_conv2d_nchw(batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups, add_bias=False, add_relu=False):
    print("Workload: (%d, %d, %d, %d, %d, %d, %d, %d, %d)" %
        (batch, in_channel, in_size, num_filter,
         kernel, stride, padding, dilation, groups))

    in_height = in_width = in_size

    A = tvm.placeholder((batch, in_channel, in_height, in_width), name='A')
    W = tvm.placeholder((num_filter, in_channel // groups, kernel, kernel), name='W')
    bias = tvm.placeholder((num_filter, 1, 1), name='bias')

    a_shape = get_const_tuple(A.shape)
    w_shape = get_const_tuple(W.shape)
    bias_shape = get_const_tuple(bias.shape)
    dtype = A.dtype

    @memoize("topi.tests.test_topi_group_conv2d.verify_group_conv2d_nchw")
    def get_ref_data():
        a_np = np.random.uniform(size=a_shape).astype(dtype)
        w_np = np.random.uniform(size=w_shape).astype(dtype)
        b_np = np.random.uniform(size=bias_shape).astype(dtype)
        dw_np = topi.testing.dilate_python(w_np, (1, 1, dilation, dilation))
        c_np = topi.testing.conv2d_nchw_python(a_np, dw_np, stride, padding, groups).astype(dtype)

        if add_bias:
            b_np = np.random.uniform(size=bias_shape).astype(dtype)
            c_np += b_np
        if add_relu:
            c_np = np.maximum(c_np, 0)

        return a_np, w_np, b_np, c_np

    a_np, w_np, b_np, c_np = get_ref_data()

    def check_device(device):
        ctx = tvm.context(device, 0)
        if not ctx.exist:
            print("Skip because %s is not enabled" % device)
            return

        print("Running on target: %s" % device)
        with tvm.target.create(device):
            C = topi.nn.group_conv2d_nchw(A, W, stride, padding, dilation, groups, out_dtype=dtype)
            if add_bias:
                C = topi.add(C, bias)
            if add_relu:
                C = topi.nn.relu(C)
            s = topi.generic.schedule_group_conv2d_nchw([C])

        a = tvm.nd.array(a_np, ctx)
        w = tvm.nd.array(w_np, ctx)
        b = tvm.nd.array(b_np, ctx)
        c = tvm.nd.array(np.zeros(get_const_tuple(C.shape), dtype=C.dtype), ctx)
        if add_bias:
            func = tvm.build(s, [A, W, bias, C], device, name="relu_%d_%d_%d_%d_%d_%d_%d_%d_%d" %\
                (batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups))
            func(a, w, b, c)
        else:
            func = tvm.build(s, [A, W, C], device, name="relu_%d_%d_%d_%d_%d_%d_%d_%d_%d" % \
            (batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups))
            func(a, w, c)
        tvm.testing.assert_allclose(c.asnumpy(), c_np, rtol=1e-5)

    for device in ["llvm", "cuda"]:
        check_device(device)


oc_block_factor = 4


def verify_group_conv2d_NCHWc_int8(batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups, add_bias=False, add_relu=False):
    print("Workload: (%d, %d, %d, %d, %d, %d, %d, %d, %d)" %
        (batch, in_channel, in_size, num_filter,
         kernel, stride, padding, dilation, groups))

    in_height = in_width = in_size

    A = tvm.placeholder((batch, in_channel, in_height, in_width), name='A', dtype='int8')
    W = tvm.placeholder((num_filter, in_channel // groups, kernel, kernel), name='W', dtype='int8')
    bias = tvm.placeholder((num_filter // oc_block_factor, 1, 1, oc_block_factor), name='bias',
                            dtype='int8')

    a_shape = get_const_tuple(A.shape)
    w_shape = get_const_tuple(W.shape)
    bias_shape = get_const_tuple(bias.shape)
    dtype = A.dtype

    @memoize("topi.tests.test_topi_group_conv2d.verify_group_conv2d_NCHWc_int8")
    def get_ref_data():
        a_np = np.random.randint(low=-128, high=127, size=a_shape).astype(dtype)
        w_np = np.random.randint(low=-128, high=128, size=w_shape).astype(dtype)
        b_np = np.random.uniform(size=bias_shape).astype(dtype)
        dw_np = topi.testing.dilate_python(w_np, (1, 1, dilation, dilation))
        c_np = topi.testing.conv2d_nchw_python(a_np, dw_np, stride, padding, groups).astype(dtype)

        # convert to NCHWc
        _, _, out_height, out_width = c_np.shape
        c_np = c_np.reshape((batch, num_filter // oc_block_factor, oc_block_factor, \
                out_height, out_width)).transpose(0, 1, 3, 4, 2)

        if add_bias:
            b_np = np.random.uniform(size=bias_shape).astype(dtype)
            c_np += b_np
        if add_relu:
            c_np = np.maximum(c_np, 0)

        return a_np, w_np, b_np, c_np

    a_np, w_np, b_np, c_np = get_ref_data()

    def check_device(device):
        ctx = tvm.context(device, 0)
        if not ctx.exist:
            print("Skip because %s is not enabled" % device)
            return
        if device == "cuda" and not tvm.contrib.nvcc.have_int8(ctx.compute_version):
            print("Skip because int8 intrinsics are not available")
            return

        print("Running on target: %s" % device)
        with tvm.target.create(device):
            C = topi.nn.group_conv2d_nchw(A, W, stride, padding, dilation, groups, out_dtype=dtype)
            if add_bias:
                C = topi.add(C, bias)
            if add_relu:
                C = topi.nn.relu(C)
            s = topi.generic.schedule_group_conv2d_nchw([C])

        a = tvm.nd.array(a_np, ctx)
        w = tvm.nd.array(w_np, ctx)
        b = tvm.nd.array(b_np, ctx)
        c = tvm.nd.array(np.zeros(get_const_tuple(C.shape), dtype=C.dtype), ctx)
        if add_bias:
            func = tvm.build(s, [A, W, bias, C], device, name="relu_%d_%d_%d_%d_%d_%d_%d_%d_%d" %\
                (batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups))
            func(a, w, b, c)
        else:
            func = tvm.build(s, [A, W, C], device, name="relu_%d_%d_%d_%d_%d_%d_%d_%d_%d" % \
            (batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation, groups))
            func(a, w, c)
        tvm.testing.assert_allclose(c.asnumpy(), c_np, rtol=1e-5)

    for device in ["cuda"]:
        check_device(device)


def test_group_conv2d_nchw():
    # ResNeXt-50 workload
    verify_group_conv2d_nchw(1, 128, 56, 128, 3, 1, 1, 1, 32)
    verify_group_conv2d_nchw(1, 256, 56, 256, 3, 2, 1, 1, 32)
    verify_group_conv2d_nchw(1, 256, 28, 256, 3, 1, 1, 1, 32)
    verify_group_conv2d_nchw(1, 512, 28, 512, 3, 2, 1, 1, 32)
    verify_group_conv2d_nchw(1, 512, 14, 512, 3, 1, 1, 1, 32)
    verify_group_conv2d_nchw(1, 1024, 14, 1024, 3, 2, 1, 1, 32)
    verify_group_conv2d_nchw(1, 1024, 7, 1024, 3, 1, 1, 1, 32)

    # bias, relu
    verify_group_conv2d_nchw(1, 128, 56, 128, 3, 1, 1, 1, 32, add_relu=True)
    verify_group_conv2d_nchw(1, 128, 56, 128, 3, 1, 1, 1, 32, add_bias=True)
    verify_group_conv2d_nchw(1, 128, 56, 128, 3, 1, 1, 1, 32, add_relu=True,
                             add_bias=True)

    # dilation
    verify_group_conv2d_nchw(1, 128, 56, 128, 3, 1, 1, 2, 32)

    # batch size
    verify_group_conv2d_nchw(2, 128, 56, 128, 3, 1, 1, 1, 32)
    verify_group_conv2d_nchw(9, 128, 56, 128, 3, 1, 1, 1, 32)



def test_group_conv2d_NCHWc_int8():
    with Int8Fallback():
        # ResNeXt-50 workload
        verify_group_conv2d_NCHWc_int8(1, 128, 56, 128, 3, 1, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 256, 56, 256, 3, 2, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 256, 28, 256, 3, 1, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 512, 28, 512, 3, 2, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 512, 14, 512, 3, 1, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 1024, 14, 1024, 3, 2, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(1, 1024, 7, 1024, 3, 1, 1, 1, 32)

        # bias, relu
        verify_group_conv2d_NCHWc_int8(1, 128, 56, 128, 3, 1, 1, 1, 32, add_relu=True)
        verify_group_conv2d_NCHWc_int8(1, 128, 56, 128, 3, 1, 1, 1, 32, add_bias=True)
        verify_group_conv2d_NCHWc_int8(1, 128, 56, 128, 3, 1, 1, 1, 32, add_relu=True,
                                       add_bias=True)
        # dilation
        verify_group_conv2d_NCHWc_int8(1, 128, 56, 128, 3, 1, 1, 2, 32)

        # batch size
        verify_group_conv2d_NCHWc_int8(2, 128, 56, 128, 3, 1, 1, 1, 32)
        verify_group_conv2d_NCHWc_int8(9, 128, 56, 128, 3, 1, 1, 1, 32)


if __name__ == "__main__":
    test_group_conv2d_nchw()
    test_group_conv2d_NCHWc_int8()
