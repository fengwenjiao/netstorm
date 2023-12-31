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
"""Example code to do convolution."""
import os
import numpy as np
import tvm
import topi
import topi.testing
from tvm.contrib.pickle_memoize import memoize
from topi.util import get_const_tuple


def verify_conv2d_nhwc(batch, in_channel, in_size, num_filter, kernel, stride, padding, dilation=1):
    in_height = in_width = in_size

    A = tvm.placeholder((batch, in_height, in_width, in_channel), name='A')
    W = tvm.placeholder((kernel, kernel, in_channel, num_filter), name='W')

    a_shape = get_const_tuple(A.shape)
    w_shape = get_const_tuple(W.shape)
    dtype = A.dtype

    @memoize("topi.tests.test_topi_conv2d_nhwc.verify_nhwc.v2")
    def get_ref_data():
        a_np = np.random.uniform(size=a_shape).astype(dtype)
        w_np = np.random.uniform(size=w_shape).astype(dtype)
        dw_np = topi.testing.dilate_python(w_np, (dilation, dilation, 1, 1))
        b_np = topi.testing.conv2d_nhwc_python(a_np, dw_np, stride, padding)
        return a_np, w_np, b_np
    a_np, w_np, b_np = get_ref_data()

    def check_device(device):
        if not tvm.module.enabled(device):
            print("Skip because %s is not enabled" % device)
            return
        print("Running on target: %s" % device)
        with tvm.target.create(device):
            B = topi.nn.conv2d(A, W, (stride, stride), padding,
                               (dilation, dilation), layout='NHWC', out_dtype=dtype)
            s = topi.generic.schedule_conv2d_nhwc([B])
        ctx = tvm.context(device, 0)
        a = tvm.nd.array(a_np, ctx)
        w = tvm.nd.array(w_np, ctx)
        b = tvm.nd.array(np.zeros(get_const_tuple(B.shape), dtype=B.dtype), ctx)
        func = tvm.build(s, [A, W, B], device)
        func(a, w, b)
        tvm.testing.assert_allclose(b.asnumpy(), b_np, rtol=1e-5)

    for device in ['llvm', 'cuda']:
        check_device(device)


def test_conv2d_nhwc():
    verify_conv2d_nhwc(1, 256, 32, 256, 3, 1, "SAME")
    verify_conv2d_nhwc(4, 128, 16, 128, 5, 2, "SAME")
    verify_conv2d_nhwc(4, 128, 16, 256, 5, 2, "SAME")
    verify_conv2d_nhwc(1, 256, 32, 256, 3, 1, "VALID")
    verify_conv2d_nhwc(1, 256, 32, 256, 3, 1, "VALID")
    verify_conv2d_nhwc(4, 128, 16, 128, 5, 2, "VALID")
    verify_conv2d_nhwc(4, 128, 16, 256, 5, 2, "VALID")
    verify_conv2d_nhwc(1, 128, 16, 256, 3, 2, (0, 0, 1, 1))
    verify_conv2d_nhwc(1, 128, 16, 256, 3, 2, (1, 1, 2, 2))
    verify_conv2d_nhwc(1, 128, 16, 128, 5, 2, (3, 3, 2, 2))
    verify_conv2d_nhwc(1, 128, 16, 256, 3, 2, (0, 1, 2, 3))
    # dilation = 2
    verify_conv2d_nhwc(1, 256, 32, 256, 3, 1, "SAME", dilation=2)
    verify_conv2d_nhwc(1, 256, 32, 256, 3, 1, (1, 1, 2, 2), dilation=2)


if __name__ == "__main__":
    test_conv2d_nhwc()
