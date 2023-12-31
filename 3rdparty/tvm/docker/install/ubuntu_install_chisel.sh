#!/bin/bash
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

set -e
set -u
set -o pipefail

# The https:// source added below required an apt https transport
# support.
apt-get update && apt-get install -y apt-transport-https flex bison

# Install the necessary dependencies for Chisel
echo "deb https://dl.bintray.com/sbt/debian /" | tee -a /etc/apt/sources.list.d/sbt.list
apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 2EE0EA64E40A89B84B2DF73499E82A75642AC823

# Note: The settings in vta/hardware/chisel/project/build.properties
# file determines required sbt version.
apt-get update && apt-get install -y sbt=1.1.1

# Install the Verilator with major version 4.0
wget https://www.veripool.org/ftp/verilator-4.010.tgz
tar xf verilator-4.010.tgz
cd verilator-4.010/
./configure
make -j4
make install
