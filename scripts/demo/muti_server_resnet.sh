#!/bin/bash

DMLC_PS_ROOT_PORT=9090
DMLC_NUM_SERVER=9
DMLC_NUM_WORKER=9
PS_VERBOSE=2
DMLC_INTERFACE=eth0
CTL_LIST_ID=$1
UPDATE_RATE=0
REPORT_RATE=0
NUM_MSG_REPORT=4
PRIMARY_BUSY_BOUND=2
AUXILIARY_QUEUE_LENGTH=1
ENABLE_MUTI_TRACE=1
ENABLE_RANDOM=0
ENABLE_ORIGINAL=0
CHECK_PERIOD=2
MXNET_KVSTORE_BIGARRAY_BOUND=1000000
IN_DEGREE=5
NUM_ENABLE_SERVER=9
ENABLE_AWARENESS=1
SIZE_MSG_REPORT=4000000
SIZE_CACHE=2000000000
model=resnet50-v1

MYIP=$(ifconfig eno2 | grep 'inet' | grep -v 'inet6' | cut -c 21-31)
  echo $MYIP
	echo "start"
  docker exec -di netstorm bash -c "source /etc/profile;\
                    export DMLC_ROLE=scheduler;\
                    export DMLC_PS_ROOT_URI=172.17.33.10;\
                    export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
                    export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
                    export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
                    export DMLC_INTERFACE=eth0; \
                    export PS_VERBOSE=$PS_VERBOSE; \
                    export DMLC_NUM_GROUP=1;\
                    export CONTAINER_ID=11; \
                    export CTL_LIST_ID=$CTL_LIST_ID;\
                    export UPDATE_RATE=$UPDATE_RATE;\
                    export ENABLE_RANDOM=$ENABLE_RANDOM;\
                    export ENABLE_MUTI_TRACE=$ENABLE_MUTI_TRACE;\
                    export CHECK_PERIOD=$CHECK_PERIOD;\
                    export IN_DEGREE=$IN_DEGREE;\
                    export NUM_ENABLE_SERVER=$NUM_ENABLE_SERVER;\
                    export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
                    python ./ts-mxnet-app/main.py -n $model -g 0 >& debug.log "
  echo s1
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=eth0; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=1; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug1.log"
  echo w1
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=eth0; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=1; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug2.log"

  echo s2
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=2; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug3.log"
	echo w2
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=2; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug4.log"
  echo s3
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=3; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug5.log"
	echo w3
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=3; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug6.log"

  echo s4
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=4; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug7.log"
	echo w4
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=4; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug8.log"
  echo s5
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=5; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug9.log"
	echo w5
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=5; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug10.log"
  echo s6
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=6; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug11.log"
	echo w6
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=6; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug12.log"
  echo s7
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=7; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug13.log"
	echo w7
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=7; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug14.log"
  echo s8
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=8; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug15.log"
	echo w8
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=8; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug16.log"

  echo s9
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=server;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=9; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export AUXILIARY_QUEUE_LENGTH=$AUXILIARY_QUEUE_LENGTH;\
                      export PRIMARY_BUSY_BOUND=$PRIMARY_BUSY_BOUND;\
										  export NUM_MSG_REPORT=$NUM_MSG_REPORT;\
										  export UPDATE_RATE=$UPDATE_RATE;\
										  export REPORT_RATE=$REPORT_RATE;\
										  export SIZE_MSG_REPORT=$SIZE_MSG_REPORT;\
										  export SIZE_CACHE=$SIZE_CACHE;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug17.log"
	echo w9
	docker exec -di netstorm bash -c "source /etc/profile;\
	                    export DMLC_ROLE=worker;\
										  export DMLC_PS_ROOT_URI=172.17.33.10;\
										  export DMLC_PS_ROOT_PORT=$DMLC_PS_ROOT_PORT; \
										  export DMLC_NUM_SERVER=$DMLC_NUM_SERVER; \
										  export DMLC_NUM_WORKER=$DMLC_NUM_WORKER; \
										  export DMLC_INTERFACE=$DMLC_INTERFACE; \
										  export PS_VERBOSE=$PS_VERBOSE; \
										  export DMLC_NUM_GROUP=1;\
										  export CONTAINER_ID=9; \
										  export CTL_LIST_ID=$CTL_LIST_ID;\
										  export ENABLE_ORIGINAL=$ENABLE_ORIGINAL;\
										  export MXNET_KVSTORE_BIGARRAY_BOUND=$MXNET_KVSTORE_BIGARRAY_BOUND;\
                      export ENABLE_AWARENESS=$ENABLE_AWARENESS;\
										  python ./ts-mxnet-app/main.py -n $model -g 1 >& debug18.log"

#===================================================================================================
	echo "end!!"