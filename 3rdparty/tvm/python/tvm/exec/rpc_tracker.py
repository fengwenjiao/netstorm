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
# pylint: disable=redefined-outer-name, invalid-name
"""Tool to start RPC tracker"""
from __future__ import absolute_import

import logging
import argparse
import multiprocessing
import sys
from ..rpc.tracker import Tracker

def main(args):
    """Main funciton"""
    tracker = Tracker(args.host, port=args.port, port_end=args.port_end,
                      silent=args.silent)
    tracker.proc.join()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', type=str, default="0.0.0.0",
                        help='the hostname of the tracker')
    parser.add_argument('--port', type=int, default=9190,
                        help='The port of the RPC')
    parser.add_argument('--port-end', type=int, default=9199,
                        help='The end search port of the RPC')
    parser.add_argument('--no-fork', dest='fork', action='store_false',
                        help="Use spawn mode to avoid fork. This option \
                         is able to avoid potential fork problems with Metal, OpenCL \
                         and ROCM compilers.")
    parser.add_argument('--silent', action='store_true',
                        help="Whether run in silent mode.")

    parser.set_defaults(fork=True)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO)
    if args.fork is False:
        if sys.version_info[0] < 3:
            raise RuntimeError(
                "Python3 is required for spawn mode."
            )
        multiprocessing.set_start_method('spawn')
    else:
        if not args.silent:
            logging.info("If you are running ROCM/Metal, fork will cause "
                         "compiler internal error. Try to launch with arg ```--no-fork```")
    main(args)
