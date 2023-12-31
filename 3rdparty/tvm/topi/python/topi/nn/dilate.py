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
# pylint: disable=invalid-name
"""Dilation operators"""
from __future__ import absolute_import as _abs
import tvm
from .. import util
from .. import tag

@tvm.tag_scope(tag=tag.INJECTIVE+",dilate")
def dilate(data, strides, name="DilatedInput"):
    """Dilate data with zeros.

    Parameters
    ----------
    data : tvm.Tensor
        n-D, can be any layout.

    strides : list / tuple of n ints
        Dilation stride on each dimension, 1 means no dilation.

    name : str, optional
        The name prefix operators generated

    Returns
    -------
    Output : tvm.Tensor
        n-D, the same layout as data.
    """
    n = len(data.shape)
    if len(strides) != n:
        raise ValueError("data dimension and strides size dismatch : %d vs %d" % (
            n, len(strides)))

    out_shape = tuple(
        tvm.ir_pass.Simplify((data.shape[i] - 1) * strides[i] + 1) for i in range(n))

    def _dilate(*indices):
        not_zero = []
        index_tuple = []
        idxdiv = tvm.indexdiv
        idxmod = tvm.indexmod
        for i in range(n):
            if not util.equal_const_int(strides[i], 1):
                index_tuple.append(idxdiv(indices[i], strides[i]))
                not_zero.append(idxmod(indices[i], strides[i]).equal(0))
            else:
                index_tuple.append(indices[i])
        if not_zero:
            not_zero = tvm.all(*not_zero)
            return tvm.if_then_else(not_zero, data(*index_tuple), tvm.const(0.0, data.dtype))
        return data(*index_tuple)

    return tvm.compute(out_shape, _dilate, name=name)
