/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file utvm_timer.c
 * \brief uTVM timer API stubs for the host emulated device
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "utvm_runtime.h"

// TODO(weberlo): use this? https://stackoverflow.com/questions/5141960/get-the-current-time-in-c

int32_t UTVMTimerStart() {
  return 0;
}

void UTVMTimerStop() { }

void UTVMTimerReset() { }

uint32_t UTVMTimerRead() {
  return 1;
}

#ifdef __cplusplus
}  // TVM_EXTERN_C
#endif
