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
"""Test code for transposed convolution."""
import numpy as np
import tvm
import topi
import topi.testing
from tvm.contrib.pickle_memoize import memoize
from topi.util import get_const_tuple

from common import get_all_backend

def verify_conv2d_transpose_nchw(batch, in_channel, in_size, num_filter, kernel, stride, padding):
    in_height, in_width = in_size
    kernel_height, kernel_width = kernel
    stride_height, stride_width = stride
    pad_top, pad_left, pad_bottom, pad_right = padding

    A = tvm.placeholder((batch, in_channel, in_height, in_width), name='A')
    W = tvm.placeholder((in_channel, num_filter, kernel_height, kernel_width), name='W')

    a_shape = get_const_tuple(A.shape)
    w_shape = get_const_tuple(W.shape)
    dtype = A.dtype

    @memoize("topi.tests.test_topi_conv2d_transpose.verify_conv2d_transpose_nchw")
    def get_ref_data():
        a_np = np.random.uniform(size=a_shape).astype(dtype)
        w_np = np.random.uniform(size=w_shape).astype(dtype)
        b_np = topi.testing.conv2d_transpose_nchw_python(a_np, w_np, stride, padding)
        c_np = np.maximum(b_np, 0)
        return a_np, w_np, b_np, c_np

    a_np, w_np, b_np, c_np = get_ref_data()

    def check_device(device):
        ctx = tvm.context(device, 0)
        if not ctx.exist:
            print("Skip because %s is not enabled" % device)
            return
        print("Running on target: %s" % device)
        with tvm.target.create(device):
            B = topi.nn.conv2d_transpose_nchw(A, W,
                                              [stride_height, stride_width],
                                              [pad_top, pad_left, pad_bottom, pad_right],
                                              A.dtype)
            C = topi.nn.relu(B)
            s1 = topi.generic.schedule_conv2d_transpose_nchw([B])
            s2 = topi.generic.schedule_conv2d_transpose_nchw([C])
        a = tvm.nd.array(a_np, ctx)
        w = tvm.nd.array(w_np, ctx)
        b = tvm.nd.array(np.zeros(get_const_tuple(B.shape), dtype=B.dtype), ctx)
        c = tvm.nd.array(np.zeros(get_const_tuple(C.shape), dtype=C.dtype), ctx)

        func1 = tvm.build(s1, [A, W, B], device)
        func2 = tvm.build(s2, [A, W, C], device)
        func1(a, w, b)
        func2(a, w, c)
        tvm.testing.assert_allclose(b.asnumpy(), b_np, rtol=1e-5)
        tvm.testing.assert_allclose(c.asnumpy(), c_np, rtol=1e-5)
    for device in get_all_backend():
        check_device(device)


def test_conv2d_transpose_nchw():
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  1, (1, 1), (1, 1), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  32, (3, 3), (1, 1), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  32, (3, 3), (3, 3), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  32, (3, 3), (1, 1), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  32, (3, 3), (2, 2), (1, 1, 1, 1))
    verify_conv2d_transpose_nchw(1, 3, (224, 224),  32, (2, 2), (2, 2), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 32, (32, 32), 128, (5, 5), (1, 1), (0, 0, 0, 0))
    verify_conv2d_transpose_nchw(1, 32, (32, 32), 128, (5, 5), (2, 2), (1, 1, 1, 1))
    verify_conv2d_transpose_nchw(16, 32, (8192, 1), 8, (31, 1), (2, 1), (14, 0, 15, 0))
    verify_conv2d_transpose_nchw(16, 512, (8, 1), 128, (31, 1), (2, 1), (14, 0, 15, 0))

if __name__ == "__main__":
    test_conv2d_transpose_nchw()
