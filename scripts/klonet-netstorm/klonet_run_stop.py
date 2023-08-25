from vemu_api import *
from time import sleep

if_stop_proc = True


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

    if if_stop_proc:
        stop_dict = {}
        stop_cmd = "ps -ef|grep \"^root.*python.*\.py.*$\"|cut -c 10-15 | xargs kill -9"
        bash_stop_cmd = f'bash -c \'{stop_cmd}\''
        for i in range(1, 12):
            stop_dict[f"h{i}"] = [bash_stop_cmd]
        print("stop!")
        exec_result = cmd_manager.exec_cmds_in_nodes(stop_dict)
        print(exec_result)
        print("end!")
