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
"""TVM operator upsampling compute."""
from __future__ import absolute_import
import topi
import tvm
from ..util import simplify


def upsampling(data, scale_h, scale_w, layout="NCHW", method='nearest_neighbor',
               align_corners=False):
    """Perform upsampling on the data.
       Nearest neighbor and bilinear upsampling are supported.

    Parameters
    ----------
    inputs : tvm.Tensor
        inputs is a 4-D tensor with shape
        [batch, channel, in_height, in_width]
        or  [batch, in_height, in_width, channel]

    scale_h : float
        Scaling factor for height

    scale_w : float
        Scaling factor for width

    layout : string, optional
        either "NCHW" or "NHWC"

    method : {"bilinear", "nearest_neighbor", "bicubic"}
        Method to be used for upsampling.

    Returns
    -------
    output : tvm.Tensor
        4-D with shape [batch, channel, in_height*scale_h, in_width*scale_w]
        or [batch, in_height*scale, in_width*scale, channel]
    """
    base_layout = layout[0:4]
    if base_layout == "NCHW":
        out_shape = (simplify(topi.cast(tvm.round(data.shape[2] * scale_h), data.shape[2].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[3] * scale_w), data.shape[3].dtype)))
    elif layout == "NHWC":
        out_shape = (simplify(topi.cast(tvm.round(data.shape[1] * scale_h), data.shape[1].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[2] * scale_w), data.shape[2].dtype)))

    else:
        raise ValueError("not support this layout {} yet".format(layout))
    coord_trans = "align_corners" if align_corners else "asymmetric"
    return topi.image.resize(data, out_shape, layout=layout,
                             method=method, coordinate_transformation_mode=coord_trans)


def upsampling3d(data, scale_d, scale_h, scale_w, layout="NCDHW", method='nearest_neighbor',
                 coordinate_transformation_mode="half_pixel"):
    """Perform upsampling on the data.
       Nearest neighbor and bilinear upsampling are supported.

    Parameters
    ----------
    inputs : tvm.Tensor
        inputs is a 5-D tensor with shape
        [batch, channel, in_depth, in_height, in_width]
        or  [batch, in_depth, in_height, in_width, channel]

    scale_d : float
        Scaling factor for depth

    scale_h : float
        Scaling factor for height

    scale_w : float
        Scaling factor for width

    layout : string, optional
        either "NCDHW" or "NDHWC"

    method : {"trilinear", "nearest_neighbor"}
        Method to be used for upsampling.

    coordinate_transformation_mode: string, optional
        Describes how to transform the coordinate in the resized tensor
        to the coordinate in the original tensor.
        Refer to the ONNX Resize operator specification for details.
        Available options are "half_pixel", "align_corners" and "asymmetric".

    Returns
    -------
    output : tvm.Tensor
        5-D with shape [batch, channel, in_depth*scale, in_height*scale, in_width*scale]
        or [batch, in_depth*scale, in_height*scale, in_width*scale, channel]
    """
    base_layout = layout[0:5]
    if base_layout == "NCDHW":
        out_shape = (simplify(topi.cast(tvm.round(data.shape[2] * scale_d), data.shape[2].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[3] * scale_h), data.shape[3].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[4] * scale_w), data.shape[4].dtype)))
    elif layout == "NDHWC":
        out_shape = (simplify(topi.cast(tvm.round(data.shape[1] * scale_d), data.shape[1].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[2] * scale_h), data.shape[2].dtype)),
                     simplify(topi.cast(tvm.round(data.shape[3] * scale_w), data.shape[3].dtype)))

    else:
        raise ValueError("not support this layout {} yet".format(layout))
    return topi.image.resize3d(data, out_shape, layout=layout, method=method,
                               coordinate_transformation_mode=coordinate_transformation_mode)
