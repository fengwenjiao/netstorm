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
# pylint: disable=invalid-name, unused-import
"""Runtime Object API"""
from __future__ import absolute_import

import sys
import ctypes
from .. import _api_internal
from .base import _FFI_MODE, _RUNTIME_ONLY, check_call, _LIB, c_str
from .object_generic import ObjectGeneric, convert_to_object, const

IMPORT_EXCEPT = RuntimeError if _FFI_MODE == "cython" else ImportError

try:
    # pylint: disable=wrong-import-position,unused-import
    if _FFI_MODE == "ctypes":
        raise ImportError()
    if sys.version_info >= (3, 0):
        from ._cy3.core import _set_class_object
        from ._cy3.core import ObjectBase as _ObjectBase
        from ._cy3.core import _register_object
    else:
        from ._cy2.core import _set_class_object
        from ._cy2.core import ObjectBase as _ObjectBase
        from ._cy2.core import _register_object
except IMPORT_EXCEPT:
    # pylint: disable=wrong-import-position,unused-import
    from ._ctypes.function import _set_class_object
    from ._ctypes.object import ObjectBase as _ObjectBase
    from ._ctypes.object import _register_object


def _new_object(cls):
    """Helper function for pickle"""
    return cls.__new__(cls)


class Object(_ObjectBase):
    """Base class for all tvm's runtime objects."""
    def __repr__(self):
        return _api_internal._format_str(self)

    def __dir__(self):
        fnames = _api_internal._NodeListAttrNames(self)
        size = fnames(-1)
        return [fnames(i) for i in range(size)]

    def __getattr__(self, name):
        try:
            return _api_internal._NodeGetAttr(self, name)
        except AttributeError:
            raise AttributeError(
                "%s has no attribute %s" % (str(type(self)), name))

    def __hash__(self):
        return _api_internal._raw_ptr(self)

    def __eq__(self, other):
        return self.same_as(other)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __reduce__(self):
        cls = type(self)
        return (_new_object, (cls, ), self.__getstate__())

    def __getstate__(self):
        handle = self.handle
        if handle is not None:
            return {'handle': _api_internal._save_json(self)}
        return {'handle': None}

    def __setstate__(self, state):
        # pylint: disable=assigning-non-slot
        handle = state['handle']
        if handle is not None:
            json_str = handle
            other = _api_internal._load_json(json_str)
            self.handle = other.handle
            other.handle = None
        else:
            self.handle = None

    def same_as(self, other):
        """check object identity equality"""
        if not isinstance(other, Object):
            return False
        return self.__hash__() == other.__hash__()


def register_object(type_key=None):
    """register object type.

    Parameters
    ----------
    type_key : str or cls
        The type key of the node

    Examples
    --------
    The following code registers MyObject
    using type key "test.MyObject"

    .. code-block:: python

      @tvm.register_object("test.MyObject")
      class MyObject(Object):
          pass
    """
    object_name = type_key if isinstance(type_key, str) else type_key.__name__

    def register(cls):
        """internal register function"""
        if hasattr(cls, "_type_index"):
            tindex = cls._type_index
        else:
            tidx = ctypes.c_uint()
            if not _RUNTIME_ONLY:
                check_call(_LIB.TVMObjectTypeKey2Index(
                    c_str(object_name), ctypes.byref(tidx)))
            else:
                # directly skip unknown objects during runtime.
                ret = _LIB.TVMObjectTypeKey2Index(
                    c_str(object_name), ctypes.byref(tidx))
                if ret != 0:
                    return cls
            tindex = tidx.value
        _register_object(tindex, cls)
        return cls

    if isinstance(type_key, str):
        return register

    return register(type_key)


def getitem_helper(obj, elem_getter, length, idx):
    """Helper function to implement a pythonic getitem function.

    Parameters
    ----------
    obj: object
        The original object

    elem_getter : function
        A simple function that takes index and return a single element.

    length : int
        The size of the array

    idx : int or slice
        The argument passed to getitem

    Returns
    -------
    result : object
        The result of getitem
    """
    if isinstance(idx, slice):
        start = idx.start if idx.start is not None else 0
        stop = idx.stop if idx.stop is not None else length
        step = idx.step if idx.step is not None else 1
        if start < 0:
            start += length
        if stop < 0:
            stop += length
        return [elem_getter(obj, i) for i in range(start, stop, step)]

    if idx < -length or idx >= length:
        raise IndexError("Index out of range. size: {}, got index {}"
                         .format(length, idx))
    if idx < 0:
        idx += length
    return elem_getter(obj, idx)


_set_class_object(Object)
