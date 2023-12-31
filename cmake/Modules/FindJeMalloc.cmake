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


# Copyright (c)      2014 Thomas Heller
# Copyright (c) 2007-2012 Hartmut Kaiser
# Copyright (c) 2010-2011 Matt Anderson
# Copyright (c) 2011      Bryce Lelbach
#
#----
# Distributed under the Boost Software License, Version 1.0.
# Boost Software License - Version 1.0 - August 17th, 2003
#
# Permission is hereby granted, free of charge, to any person or organization
# obtaining a copy of the software and accompanying documentation covered by
# this license (the "Software") to use, reproduce, display, distribute,
# execute, and transmit the Software, and to prepare derivative works of the
# Software, and to permit third-parties to whom the Software is furnished to
# do so, all subject to the following:
#
# The copyright notices in the Software and this entire statement, including
# the above license grant, this restriction and the following disclaimer,
# must be included in all copies of the Software, in whole or in part, and
# all derivative works of the Software, unless such copies or derivative
# works are solely in the form of machine-executable object code generated by
# a source language processor.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
# SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
# FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

find_package(PkgConfig)
pkg_check_modules(PC_JEMALLOC QUIET jemalloc)

find_path(JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h
  HINTS
    ${JEMALLOC_ROOT} ENV JEMALLOC_ROOT
    ${PC_JEMALLOC_MINIMAL_INCLUDEDIR}
    ${PC_JEMALLOC_MINIMAL_INCLUDE_DIRS}
    ${PC_JEMALLOC_INCLUDEDIR}
    ${PC_JEMALLOC_INCLUDE_DIRS}
  PATH_SUFFIXES include)

find_library(JEMALLOC_LIBRARY NAMES jemalloc libjemalloc
  HINTS
    ${JEMALLOC_ROOT} ENV JEMALLOC_ROOT
    ${PC_JEMALLOC_MINIMAL_LIBDIR}
    ${PC_JEMALLOC_MINIMAL_LIBRARY_DIRS}
    ${PC_JEMALLOC_LIBDIR}
    ${PC_JEMALLOC_LIBRARY_DIRS}
  PATH_SUFFIXES lib lib64)

set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})

find_package_handle_standard_args(Jemalloc DEFAULT_MSG
  JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)

get_property(_type CACHE JEMALLOC_ROOT PROPERTY TYPE)
if(_type)
  set_property(CACHE JEMALLOC_ROOT PROPERTY ADVANCED 1)
  if("x${_type}" STREQUAL "xUNINITIALIZED")
    set_property(CACHE JEMALLOC_ROOT PROPERTY TYPE PATH)
  endif()
endif()

mark_as_advanced(JEMALLOC_ROOT JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)
