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
"""Example code to do reorg."""
import numpy as np
import topi
from topi.util import get_const_tuple
import tvm
import topi.testing

def verify_reorg(batch, in_size, in_channel, stride):
    '''Verify reorg operator by comparing outputs from tvm and numpy implementation'''
    in_height = in_width = in_size

    A = tvm.placeholder((batch, in_channel, in_height, in_width), name='A')
    B = topi.vision.reorg(A, stride)

    a_shape = get_const_tuple(A.shape)
    dtype = A.dtype

    def get_ref_data_reorg():
        a_np = np.random.uniform(size=a_shape).astype(dtype)
        b_np = topi.testing.reorg_python(a_np, stride)
        return a_np, b_np

    a_np, b_np = get_ref_data_reorg()

    def check_device(device):
        '''Cheching devices is enabled or not'''
        ctx = tvm.context(device, 0)
        if not ctx.exist:
            print("Skip because %s is not enabled" % device)
            return
        print("Running on target: %s" % device)
        with tvm.target.create(device):
            if device == 'llvm':
                s = topi.generic.schedule_reorg([B])
            else:
                s = topi.cuda.schedule_reorg([B])
        a = tvm.nd.array(a_np, ctx)
        b = tvm.nd.array(np.zeros(get_const_tuple(B.shape), dtype=B.dtype), ctx)
        func = tvm.build(s, [A, B], device)
        func(a, b)
        tvm.testing.assert_allclose(b.asnumpy(), b_np, rtol=1e-5)

    for device in ['llvm', 'cuda']:
        check_device(device)

def test_reorg():
    verify_reorg(1, 20, 8, 2)

if __name__ == "__main__":
    test_reorg()
