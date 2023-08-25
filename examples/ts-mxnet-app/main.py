import argparse
import mxnet as mx
from mxnet import init, nd
from mxnet.gluon import loss as gloss
from mxnet.gluon import Trainer
from config import *
import random
import numpy as np
from mxnet.gluon import model_zoo
import os

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--learning-rate", type=float, default=LEARNING_RATE)
    parser.add_argument("-b", "--batch-size", type=int, default=BATCH_SIZE)
    parser.add_argument("-le", "--num-local-epochs", type=int, default=NUM_LOCAL_EPOCHS)
    parser.add_argument("-te", "--num-total-epochs", type=int, default=NUM_TOTAL_EPOCHS)
    parser.add_argument("-dd", "--data-dir", type=str, default=DATA_DIR)
    parser.add_argument("-dt", "--data-type", type=str, default=DATA_TYPE)
    parser.add_argument("-g", "--gpu", type=int, default=DEFAULT_GPU_ID)
    parser.add_argument("-c", "--cpu", type=int, default=USE_CPU)
    parser.add_argument("-n", "--network", type=str, default=NETWORK)
    parser.add_argument("-ld", "--log-dir", type=str, default=LOG_DIR)
    parser.add_argument("-e", "--eval-duration", type=int, default=EVAL_DURATION)
    parser.add_argument("-f", "--framework", type=str, default=FRAMEWORK)
    parser.add_argument("-m", "--mode", type=str, default=MODE)
    parser.add_argument("-s", "--split-by-class", type=int, default=SPLIT_BY_CLASS)
    parser.add_argument("-ds", "--data-slice-idx", type=int, default=0)
    parser.add_argument("-gc", "--use-2bit-compression", type=int, default=USE_2BIT_COMPRESSION)
    args, unknown = parser.parse_known_args()

    random.seed(1)
    np.random.seed(12)
    mx.random.seed(123)

    lr = args.learning_rate
    batch_size = args.batch_size
    num_local_epochs = args.num_local_epochs
    num_total_epochs = args.num_total_epochs
    data_dir = args.data_dir
    data_type = args.data_type.lower()
    network = args.network
    eval_duration = args.eval_duration
    log_dir = args.log_dir
    ctx = mx.cpu() if args.cpu else mx.gpu(args.gpu)
    framework = args.framework
    mode = args.mode
    split_by_class = args.split_by_class
    data_slice_idx = args.data_slice_idx
    use_2bit_compression = args.use_2bit_compression

    if data_type in ["mnist", "fashion-mnist"]:
        depth = 1
        shape = (batch_size, depth, 28, 28)
    elif data_type == "cifar10":
        depth = 3
        shape = (batch_size, depth, 32, 32)
    else:
        raise NotImplementedError("Dataset %s not support." % data_type)

    net = None
    #######################################################
    if network == "resnet18-v1":
        from symbols.resnet import resnet18_v1

        net = resnet18_v1(classes=10)
    elif network == "resnet34-v1":
        from symbols.resnet import resnet34_v1

        net = resnet34_v1(classes=10)
    elif network == "resnet50-v1":
        from symbols.resnet import resnet50_v1

        net = resnet50_v1(classes=10)
    elif network == "resnet50-v2":
        from symbols.resnet import resnet50_v2

        net = resnet50_v2(classes=10)
    elif network == "resnet101-v1":
        from symbols.resnet import resnet101_v1

        net = resnet101_v1(classes=10)
    elif network == "alexnet":
        from symbols.alexnet import alexnet

        net = alexnet(classes=10)
        shape = (batch_size, depth, 224, 224)
    elif network == "mobilenet-v1":
        from symbols.mobilenet import mobilenet1_0

        net = mobilenet1_0(classes=10)
    elif network == "mobilenet-v2":
        from symbols.mobilenet import mobilenet_v2_1_0

        net = mobilenet_v2_1_0(classes=10)
    elif network == "inception-v3":
        from symbols.inception import inception_v3

        net = inception_v3(classes=10)
        shape = (batch_size, depth, 299, 299)
    elif network == "vgg11":
        from symbols.vgg import vgg11

        net = vgg11(classes=10)
        shape = (batch_size, depth, 224, 224)
    elif network == "vgg16":
        from symbols.vgg import vgg16

        net = vgg16(classes=10)
        shape = (batch_size, depth, 224, 224)
    elif network == "vgg19":
        from symbols.vgg import vgg19

        net = vgg19(classes=10)
        shape = (batch_size, depth, 224, 224)
    else:
        from symbols.simplenet import simplenet

        net = simplenet(classes=10)
    ####################################################################

    # DenseNet
    # num_layers: 121, 161, 169, 201
    # net = model_zoo.vision.densenet.get_densenet(num_layers=121, pretrained=False)
    # shape = (batch_size, 1, 224, 224)

    # Inception v3
    # net = model_zoo.vision.inception_v3(pretrained=False)
    # shape = (batch_size, 1, 299, 299)

    # VGG
    # num_layers: 11, 13, 16, 19
    # net = model_zoo.vision.get_vgg(num_layers=11, pretrained=False)
    # shape = (batch_size, 1, 224, 224)

    # Squeezenet
    # version: 1.0, 1.1
    # net = model_zoo.vision.squeezenet.get_squeezenet(version='1.1', pretrained=False)
    # shape = (batch_size, 1, 224, 224)

    # ResNet
    # version: 1, 2
    # num_layers: 18, 34, 50, 101, 152
    # net = model_zoo.vision.get_resnet(version=1, num_layers=152, pretrained=False)
    # shape = (batch_size, 1, 28, 28)

    net.initialize(init=init.Xavier(), ctx=ctx)
    net(nd.random.uniform(shape=shape, ctx=ctx))

    loss = gloss.SoftmaxCrossEntropyLoss()
    local_trainer = Trainer(net.collect_params(), "sgd", {"learning_rate": lr})

    kwargs = {
        "lr": lr,
        "batch_size": batch_size,
        "num_total_epochs": num_total_epochs,
        "data_dir": data_dir,
        "data_type": data_type,
        "log_dir": log_dir,
        "eval_duration": eval_duration,
        "ctx": ctx,
        "shape": shape,
        "net": net,
        "loss": loss,
    }

    if framework == "mxnet":
        if mode == "sync":
            from trainer.mxnet.sync import trainer

            trainer(kwargs)
    elif framework == "hips":
        kwargs.update({
            "num_local_epochs": num_local_epochs,
            "trainer": local_trainer,
            "split_by_class": split_by_class,
            "data_slice_idx": data_slice_idx,
            "use_2bit_compression": use_2bit_compression
        })
        if mode == "sync":
            from trainer.hips.sync import trainer

            trainer(kwargs)
        elif mode == "fedavg-sync":
            from trainer.hips.fedavg_sync import trainer

            trainer(kwargs)
        elif mode == "fedavg-async":
            from trainer.hips.fedavg_async import trainer

            kwargs.update({
                "use_dcasgd": use_dcasgd
            })
            trainer(kwargs)
