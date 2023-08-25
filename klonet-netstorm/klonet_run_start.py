from vemu_api import *
from time import sleep

DMLC_PS_ROOT_PORT = 9090
DMLC_NUM_SERVER = 9
DMLC_NUM_WORKER = 9
PS_VERBOSE = 2
DMLC_INTERFACE = "toh1"
CTL_LIST_ID = "$1"
PRIMARY_BUSY_BOUND = 2
AUXILIARY_QUEUE_LENGTH = 1
UPDATE_RATE = 0
REPORT_RATE = 0
NUM_MSG_REPORT = 4
ENABLE_MUTI_TRACE = 0
ENABLE_RANDOM = 0
ENABLE_ORIGINAL = 0
CHECK_PERIOD = 5
MXNET_KVSTORE_BIGARRAY_BOUND = 1000000
IN_DEGREE = 5
NUM_ENABLE_SERVER = 9
ENABLE_AWARENESS = 0
SIZE_MSG_REPORT = 2000000
SIZE_CACHE = 2000000000
model = "alexnet"
# alexnet
# resnet50-v1
# mobilenet-v1

# resnet18-v1
# resnet34-v1
# resnet101-v1
# vgg16
# vgg19
if_exec_cmds = True

topo = Topo()


def route_set(node1, node2, name1: int, name2: int, num: int):
    topo.add_link(node1, node2, f"h{name1}-h{name2}", src_IP=f"10.0.{num}.{name2}/24", dst_IP=f"10.0.{num}.{name1}/24")
    sleep(1)


if __name__ == "__main__":
    user_name = "wudx"
    project_name = "netstorm"

    backend_ip = "192.168.1.16"
    backend_port = 22222

    image_manager = ImageManager(user_name, backend_ip, backend_port)
    project_manager = ProjectManager(user_name, backend_ip, backend_port)
    node_manager = NodeManager(user_name, project_name, backend_ip,
                               backend_port)
    link_manager = LinkManager(user_name, project_name, backend_ip,
                               backend_port)
    cmd_manager = CmdManager(user_name, project_name, backend_ip, backend_port)

    images = image_manager.get_images()
    node_image = images["nvidia_mxnet17_image_latest"]

    # ==================hyperparameter setting===================
    cmd_dict = {}
    cmd_another_dict = {}
    cmd_scheduler_dict = {}
    # h11
    cmd_scheduler_dict["h11"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=scheduler;\
                export DMLC_PS_ROOT_URI=10.0.10.10;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE=toh0; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=11; \
                export UPDATE_RATE={UPDATE_RATE};\
                export ENABLE_RANDOM={ENABLE_RANDOM};\
                export ENABLE_MUTI_TRACE={ENABLE_MUTI_TRACE};\
                export CHECK_PERIOD={CHECK_PERIOD};\
                export IN_DEGREE={IN_DEGREE};\
                export NUM_ENABLE_SERVER={NUM_ENABLE_SERVER};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 0  >& debug.log "']
    # h1
    cmd_dict["h1"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.1.1;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE=toh2; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=1; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug1.log"']
    cmd_another_dict["h1"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.1.1;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE=toh2; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=1; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug2.log"']
    # h2
    cmd_dict["h2"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.2.2;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=2; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug1.log"']
    cmd_another_dict["h2"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.2.2;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=2; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug2.log"']
    # h3
    cmd_dict["h3"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.3.3;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=3; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug1.log"']
    cmd_another_dict["h3"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.3.3;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=3; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 1 >& debug2.log"']
    # h4
    cmd_dict["h4"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.4.4;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=4; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug1.log"']
    cmd_another_dict["h4"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.4.4;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=4; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug2.log"']
    # h5
    cmd_dict["h5"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.5.5;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=5; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug1.log"']
    cmd_another_dict["h5"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.5.5;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=5; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug2.log"']
    # h6
    cmd_dict["h6"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.6.6;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=6; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug1.log"']
    cmd_another_dict["h6"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.6.6;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=6; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 2 >& debug2.log"']
    # h7
    cmd_dict["h7"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.7.7;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=7; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug1.log"']
    cmd_another_dict["h7"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.7.7;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=7; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug2.log"']
    # h8
    cmd_dict["h8"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.8.8;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=8; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug1.log"']
    cmd_another_dict["h8"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.8.8;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=8; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug2.log"']
    # h9
    cmd_dict["h9"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=server;\
                export DMLC_PS_ROOT_URI=10.0.9.9;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=9; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export AUXILIARY_QUEUE_LENGTH={AUXILIARY_QUEUE_LENGTH};\
                export PRIMARY_BUSY_BOUND={PRIMARY_BUSY_BOUND};\
                export NUM_MSG_REPORT={NUM_MSG_REPORT};\
                export UPDATE_RATE={UPDATE_RATE};\
                export REPORT_RATE={REPORT_RATE};\
                export SIZE_MSG_REPORT={SIZE_MSG_REPORT};\
                export SIZE_CACHE={SIZE_CACHE};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug1.log"']
    cmd_another_dict["h9"] = [f'bash -c "source /etc/profile;\
                export DMLC_ROLE=worker;\
                export DMLC_PS_ROOT_URI=10.0.9.9;\
                export DMLC_PS_ROOT_PORT={DMLC_PS_ROOT_PORT}; \
                export DMLC_NUM_SERVER={DMLC_NUM_SERVER}; \
                export DMLC_NUM_WORKER={DMLC_NUM_WORKER}; \
                export DMLC_INTERFACE={DMLC_INTERFACE}; \
                export PS_VERBOSE={PS_VERBOSE}; \
                export DMLC_NUM_GROUP=1;\
                export CONTAINER_ID=9; \
                export CTL_LIST_ID={CTL_LIST_ID};\
                export ENABLE_ORIGINAL={ENABLE_ORIGINAL};\
                export MXNET_KVSTORE_BIGARRAY_BOUND={MXNET_KVSTORE_BIGARRAY_BOUND};\
                export ENABLE_AWARENESS={ENABLE_AWARENESS};\
                python ./ts-mxnet-app/main.py -n {model} -g 3 >& debug2.log"']
    # Execute start command
    if if_exec_cmds:
        print("start!")
        exec_scheduler_result = cmd_manager.exec_cmds_in_nodes(cmd_scheduler_dict)  # scheduler
        print(exec_scheduler_result)
        exec_result = cmd_manager.exec_cmds_in_nodes(cmd_dict)  # server
        print(exec_result)
        exec_another_result = cmd_manager.exec_cmds_in_nodes(cmd_another_dict)  # worker
        print(exec_another_result)
        print("end!")


