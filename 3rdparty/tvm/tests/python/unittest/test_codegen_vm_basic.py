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
import tvm
import numpy as np

def run_jit(fapi, check):
    for target in ["llvm", "stackvm"]:
        if not tvm.module.enabled(target):
            continue
        f = tvm.codegen.build_module(fapi, target)
        s = f.get_source()
        check(f)

def test_stack_vm_basic():
    a = tvm.nd.array(np.zeros(10, dtype='float32'))
    @tvm.register_func
    def tvm_call_back_get_shape(shape0):
        print(shape0)
        assert shape0 == a.shape[0]

    n = tvm.size_var('n')
    Ab = tvm.decl_buffer((n, ), tvm.float32)
    stmt = tvm.make.Evaluate(tvm.call_packed("tvm_call_back_get_shape", Ab.shape[0]))
    fapi = tvm.ir_pass.MakeAPI(stmt, "print_shape", [Ab], 0, True)
    fapi = tvm.ir_pass.LowerTVMBuiltin(fapi)
    fapi = tvm.ir_pass.LowerIntrin(fapi, "stackvm")
    run_jit(fapi, lambda f: f(a))


@tvm.register_func
def tvm_stack_vm_print(*x):
    print(x)

def test_stack_vm_loop():
    dtype = 'int64'
    n = tvm.size_var('n')
    Ab = tvm.decl_buffer((n, ), dtype)
    i = tvm.size_var('i')

    ib = tvm.ir_builder.create()
    A = ib.buffer_ptr(Ab)
    with ib.for_range(0, n - 1, "i") as i:
        A[i + 1] = A[i] + 1
        ib.emit(tvm.call_packed("tvm_stack_vm_print", i))

    stmt = ib.get()
    fapi = tvm.ir_pass.MakeAPI(stmt, "ramp", [Ab], 0, True)
    fapi = tvm.ir_pass.LowerTVMBuiltin(fapi)
    a = tvm.nd.array(np.zeros(10, dtype=dtype))
    def check(f):
        f(a)
        np.testing.assert_equal(a.asnumpy(), np.arange(a.shape[0]))
    run_jit(fapi, check)


def test_stack_vm_cond():
    dtype = 'int64'
    n = tvm.size_var('n')
    Ab = tvm.decl_buffer((n, ), dtype)

    ib = tvm.ir_builder.create()
    A = ib.buffer_ptr(Ab)
    with ib.for_range(0, n - 1, "i") as i:
        with ib.if_scope(tvm.make.EQ(i,  4)):
            A[i + 1] = A[i] + 1
        with ib.else_scope():
            A[i + 1] = A[i] + 2

    stmt = ib.get()
    fapi = tvm.ir_pass.MakeAPI(stmt, "test", [Ab], 0, True)
    fapi = tvm.ir_pass.LowerTVMBuiltin(fapi)
    def check(f):
        a = tvm.nd.array(np.zeros(10, dtype=dtype))
        f(a)
        y = np.arange(a.shape[0]) * 2
        y[5:] -= 1
        np.testing.assert_equal(a.asnumpy(), y)
    run_jit(fapi, check)

def test_vm_parallel():
    dtype = 'int64'
    n = tvm.size_var('n')
    Ab = tvm.decl_buffer((n, ), dtype)
    i = tvm.size_var('i')
    ib = tvm.ir_builder.create()
    A = ib.buffer_ptr(Ab)
    with ib.for_range(0, n, "i", for_type="parallel") as i:
        A[i] = A[i] + 1
    stmt = ib.get()
    fapi = tvm.ir_pass.MakeAPI(stmt, "ramp", [Ab], 0, True)
    fapi = tvm.ir_pass.LowerTVMBuiltin(fapi)
    def check(f):
        a = tvm.nd.array(np.zeros(10, dtype=dtype))
        f(a)
        np.testing.assert_equal(a.asnumpy(), np.ones(a.shape[0]))
    run_jit(fapi, check)


if __name__ == "__main__":
    test_vm_parallel()
    test_stack_vm_loop()
    test_stack_vm_basic()
    test_stack_vm_cond()
