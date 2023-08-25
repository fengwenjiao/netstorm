#!/bin/bash

cd <path-to-examples>

#h11
sudo docker cp ts-mxnet-app {netstorm-trainer11-ID}:/root/
echo h11
#h10
sudo docker cp ts-mxnet-app {netstorm-trainer10-ID}:/root/
echo h10
#h9
sudo docker cp ts-mxnet-app {netstorm-trainer9-ID}:/root/
echo h9
#h8
sudo docker cp ts-mxnet-app {netstorm-trainer8-ID}:/root/
echo h8
#h7
sudo docker cp ts-mxnet-app {netstorm-trainer7-ID}:/root/
echo h7
#h6
sudo docker cp ts-mxnet-app {netstorm-trainer6-ID}:/root/
echo h6
#h5
sudo docker cp ts-mxnet-app {netstorm-trainer5-ID}:/root/
echo h5
#h4
sudo docker cp ts-mxnet-app {netstorm-trainer4-ID}:/root/
echo h4
#h3
sudo docker cp ts-mxnet-app {netstorm-trainer3-ID}:/root/
echo h3
#h2
sudo docker cp ts-mxnet-app {netstorm-trainer2-ID}:/root/
echo h2
#h1
sudo docker cp ts-mxnet-app {netstorm-trainer1-ID}:/root/ 
echo h1