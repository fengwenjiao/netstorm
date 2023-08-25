#!/bin/bash

cd <path-to-netstorm> && mkdir lib

sudo docker cp netstorm-sync:/root/incubator-mxnet/lib/libmxnet.so lib/

echo "netstorm-sync"

sudo docker cp lib/libmxnet.so {netstorm-trainer1-ID}:/root/mxnet/
echo h1

sudo docker cp lib/libmxnet.so {netstorm-trainer2-ID}:/root/mxnet/
echo h2

sudo docker cp lib/libmxnet.so {netstorm-trainer3-ID}:/root/mxnet/
echo h3

sudo docker cp lib/libmxnet.so {netstorm-trainer4-ID}:/root/mxnet/
echo h4

sudo docker cp lib/libmxnet.so {netstorm-trainer5-ID}:/root/mxnet/
echo h5

sudo docker cp lib/libmxnet.so {netstorm-trainer6-ID}:/root/mxnet/
echo h6

sudo docker cp lib/libmxnet.so {netstorm-trainer7-ID}:/root/mxnet/
echo h7

sudo docker cp lib/libmxnet.so {netstorm-trainer8-ID}:/root/mxnet/
echo h8

sudo docker cp lib/libmxnet.so {netstorm-trainer9-ID}:/root/mxnet/
echo h9

sudo docker cp lib/libmxnet.so {netstorm-trainer10-ID}:/root/mxnet/
echo h10

sudo docker cp lib/libmxnet.so {netstorm-trainer11-ID}:/root/mxnet/
echo h11


