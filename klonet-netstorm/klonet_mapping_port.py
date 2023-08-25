from vemu_api import *
from time import sleep


if_mapping_port = True
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

    # ==================port mapping==================
    if if_mapping_port:
        # add port mapping
        for i in range(1, 12):
            res = node_manager.modify_port_mapping(f"h{i}", [9380 + i, 9380 + i, 22, 9280 + i])
        print(res)
