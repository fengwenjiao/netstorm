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
from tvm import relay
from tvm.relay.testing.temp_op_attr import TempOpAttr

def test_op_attr():
    log_op = relay.op.get("log")

    @relay.op.register("exp", "ftest")
    def test(x):
        return x + 1

    assert log_op.num_inputs  == 1
    assert log_op.get_attr("ftest") is None
    assert relay.op.get("exp").get_attr("ftest")(1) == 2

def test_op_reset_attr():
    """ Tests reset_attr functionality. """
    def add1(x):
        return x + 1

    def add2(x):
        return x + 2

    # Register fadd1 and fadd2 attributes.
    relay.op.register("exp", "fadd1", add1)
    relay.op.register("log", "fadd1", add1)
    relay.op.register("log", "fadd2", add2)

    # Reset log fadd1 attr.
    log_op = relay.op.get("log")
    log_op.reset_attr("fadd1")

    # Check that fadd1 attr is resetted.
    assert log_op.get_attr("fadd1") is None

    # Check that fadd1 attr of other ops are intact.
    assert relay.op.get("exp").get_attr("fadd1")(1) == 2

    # Check that other attrs of the log op are intact.
    assert relay.op.get("log").get_attr("fadd2")(1) == 3

def test_op_temp_attr():
    """ Tests reset_attr functionality. """
    def add1(x):
        return x + 1

    def add2(x):
        return x + 2

    # Set original attr value is add1.
    relay.op.register("sqrt", "ftest", add1)

    with TempOpAttr("sqrt", "ftest", add2):
        # Check that the attr value is updated to add2.
        assert relay.op.get("sqrt").get_attr("ftest")(1) == 3

    # Check that the attr value is recovered to add1.
    assert relay.op.get("sqrt").get_attr("ftest")(1) == 2

def test_op_level1():
    x = relay.Var("x")

    for op_name in ["log", "exp", "sqrt", "rsqrt","tanh"]:
        y = getattr(relay, op_name)(x)
        assert y.op.name == op_name
        assert y.op.support_level == 1
        assert y.args[0] == x

def test_op_level3():
    x = relay.Var("x")

    for op_name in ["ceil", "floor", "trunc", "round", "abs", "negative"]:
        y = getattr(relay, op_name)(x)
        assert y.op.name == op_name
        assert y.op.support_level == 3
        assert y.args[0] == x

if __name__ == "__main__":
    test_op_attr()
    test_op_reset_attr()
    test_op_temp_attr()
    test_op_level1()
    test_op_level3()
