from vemu_api import *
from time import sleep

if_deploy = True
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

    # ==================deploy topo==================
    if if_deploy:
        # add nodes
        node_list = []
        for i in range(1, 12):
            h = topo.add_node(node_image, node_name=f"h{i}")
            node_list.append(h)
        print(topo.get_nodes())
        # add links
        # ll-h11
        h11 = node_list[10]
        num = 1
        for h in node_list[0:10]:
            route_set(h11, h, 11, num, num)
            num = num + 1
        # ll-h10
        h10 = node_list[9]
        num = 1
        for h in node_list[0:8]:
            route_set(h10, h, 10, num, num + 1)
            num = num + 1
        route_set(h10, node_list[8], 10, 9, 1)
        # ll-h1
        route_set(node_list[0], node_list[1], 1, 2, 4)
        route_set(node_list[0], node_list[2], 1, 3, 5)
        # ll-h2
        route_set(node_list[1], node_list[2], 2, 3, 1)
        route_set(node_list[1], node_list[4], 2, 5, 7)
        # ll-h3
        route_set(node_list[2], node_list[3], 3, 4, 2)
        # ll-h4
        route_set(node_list[3], node_list[4], 4, 5, 1)
        route_set(node_list[3], node_list[5], 4, 6, 3)
        # ll-h5
        route_set(node_list[4], node_list[6], 5, 7, 2)
        # ll-h6
        route_set(node_list[5], node_list[6], 6, 7, 1)
        route_set(node_list[5], node_list[7], 6, 8, 2)
        route_set(node_list[5], node_list[8], 6, 9, 4)
        # ll-h7
        route_set(node_list[6], node_list[7], 7, 8, 4)
        # ll-h8
        route_set(node_list[7], node_list[8], 8, 9, 5)
        # ll-h9

        # deploy topo
        project_manager.deploy("netstorm", topo)
        print(topo.get_nodes())
