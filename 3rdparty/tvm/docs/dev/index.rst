..  Licensed to the Apache Software Foundation (ASF) under one
    or more contributor license agreements.  See the NOTICE file
    distributed with this work for additional information
    regarding copyright ownership.  The ASF licenses this file
    to you under the Apache License, Version 2.0 (the
    "License"); you may not use this file except in compliance
    with the License.  You may obtain a copy of the License at

..    http://www.apache.org/licenses/LICENSE-2.0

..  Unless required by applicable law or agreed to in writing,
    software distributed under the License is distributed on an
    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
    KIND, either express or implied.  See the License for the
    specific language governing permissions and limitations
    under the License.

Design and Developer Guide
==========================

Building a compiler stack for deep learning systems involves many many systems-level design decisions.
In this part of documentation, we share the rationale for the specific choices made when designing TVM.

.. toctree::
   :maxdepth: 2

   runtime
   debugger
   hybrid_script
   relay_intro
   relay_add_op
   relay_pass_infra
   relay_add_pass
   relay_bring_your_own_codegen
   virtual_machine
   codebase_walkthrough
   convert_layout
   inferbound
   benchmark
