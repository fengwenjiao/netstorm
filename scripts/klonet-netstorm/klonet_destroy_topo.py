from vemu_api import *

if_destroy = True

topo = Topo()

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

    # ==================destroy topology===================
    if if_destroy:
        project_manager.destroy("netstorm")
