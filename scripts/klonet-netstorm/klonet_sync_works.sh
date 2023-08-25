#!/bin/bash
#dir=ll-v2-jqx

cd <path-to-netstorm>

#sync kv_app.h
sudo docker cp 3rdparty/ps-lite/include/ps/kv_app.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/
#sync simple_app.h
sudo docker cp 3rdparty/ps-lite/include/ps/simple_app.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/
#sync van.h
sudo docker cp 3rdparty/ps-lite/include/ps/internal/van.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/internal/
#sync threadsafe_queue.h
sudo docker cp 3rdparty/ps-lite/include/ps/internal/threadsafe_queue.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/internal/
#sync customer.h
sudo docker cp 3rdparty/ps-lite/include/ps/internal/customer.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/internal/
#sync customer.cc
sudo docker cp 3rdparty/ps-lite/src/customer.cc netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/src/
#sync van.cc
sudo docker cp 3rdparty/ps-lite/src/van.cc netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/src/
#sync zmq_van.h
sudo docker cp 3rdparty/ps-lite/src/zmq_van.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/src/
#sync meta.proto
sudo docker cp 3rdparty/ps-lite/src/meta.proto netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/src/
#sync message.h
sudo docker cp 3rdparty/ps-lite/include/ps/internal/message.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/include/ps/internal/
#sync resender.h
sudo docker cp 3rdparty/ps-lite/src/resender.h netstorm-sync:/root/incubator-mxnet/3rdparty/ps-lite/src/
#sync kvstore_dist.h
sudo docker cp src/kvstore/kvstore_dist.h netstorm-sync:/root/incubator-mxnet/src/kvstore/
#sync kvstore_dist_server.h
sudo docker cp src/kvstore/kvstore_dist_server.h netstorm-sync:/root/incubator-mxnet/src/kvstore/

echo "sync finished!"
echo "make begin!"
sudo docker exec -it netstorm-sync /bin/bash -c "cd ~/incubator-mxnet/ ;\
                                        make -j48"
echo "make over!"
