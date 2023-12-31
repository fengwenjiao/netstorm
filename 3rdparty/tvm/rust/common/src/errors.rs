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

#[derive(Debug, Fail)]
#[fail(
    display = "Could not downcast `{}` into `{}`",
    expected_type, actual_type
)]
pub struct ValueDowncastError {
    pub actual_type: String,
    pub expected_type: &'static str,
}

#[derive(Debug, Fail)]
#[fail(display = "Function call `{}` returned error: {}", context, message)]
pub struct FuncCallError {
    context: String,
    message: String,
}

impl FuncCallError {
    pub fn get_with_context(context: String) -> Self {
        Self {
            context,
            message: unsafe { std::ffi::CStr::from_ptr(crate::ffi::TVMGetLastError()) }
                .to_str()
                .expect("double fault")
                .to_owned(),
        }
    }
}
