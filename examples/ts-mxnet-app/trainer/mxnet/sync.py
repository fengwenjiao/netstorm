import os
import time
import numpy as np
import mxnet as mx
from mxnet import kv, nd, autograd
from utils import load_data, get_batch, eval_acc, Measure


def trainer(kwargs):
    lr = kwargs["lr"]
    batch_size = kwargs["batch_size"]
    num_total_epochs = kwargs["num_total_epochs"]
    data_dir = kwargs["data_dir"]
    data_type = kwargs["data_type"]
    log_dir = kwargs["log_dir"]
    eval_duration = kwargs["eval_duration"]
    ctx = kwargs["ctx"]
    shape = kwargs["shape"]
    net = kwargs["net"]
    loss = kwargs["loss"]

    if not os.path.exists(log_dir):
        os.mkdir(log_dir)

    fp = open(os.path.join(log_dir, "run.log"), "w")
    fp.write("begin running!!!\n")
    fp.close()

    kvstore_dist = kv.create("dist_sync")
    rank = kvstore_dist.rank
    trainer = mx.gluon.Trainer(net.collect_params(), 'adam', {'learning_rate': lr})
    num_workers = kvstore_dist.num_workers

    params = list(net.collect_params().values())
    for idx, param in enumerate(params):
        if param.grad_req == "null":
            continue
        kvstore_dist.init(idx, param.data())
        kvstore_dist.pull(idx, param.data(), priority=-idx)
        param.data().wait_to_read()
    nd.waitall()

    fp = open(os.path.join(log_dir, "run.log"), "a")
    fp.write("init finish\n")
    fp.close()

    train_iter, test_iter, num_train_samples, num_test_samples = load_data(
        batch_size,
        num_workers,
        rank,
        data_type=data_type,
        resize=shape[-2:],
        root=data_dir)
    num_batch_per_epoch = np.ceil(num_train_samples / num_workers / batch_size)

    if not os.path.exists(log_dir):
        os.mkdir(log_dir)

    global_iters = 1
    num_epoch = 0
    begin_time = time.time()
    eval_time = 0

    while num_epoch < num_total_epochs:
        for _, batch in enumerate(train_iter):
            Xs, ys, num_samples = get_batch(batch, ctx)
            ls = []
            with autograd.record():
                y_hats = [net(X) for X in Xs]
                ls = [loss(y_hat, y) for y_hat, y in zip(y_hats, ys)]
            for l in ls:
                l.backward()

            for idx, param in enumerate(params):  # idex=模型的层号
                if param.grad_req == "null":
                    continue
                kvstore_dist.push(idx, param.grad() / num_samples, priority=-idx)

            for idx, param in enumerate(params):
                if param.grad_req == "null":
                    continue
                temp = nd.zeros(param.shape, ctx=ctx)
                kvstore_dist.pull(idx, temp, priority=-idx)
                temp.wait_to_read()
                param.grad()[:] = temp

            nd.waitall()  # 4

            trainer.step(num_workers)
            if global_iters % 1 == 0:
                # test_acc = eval_acc(test_iter, net, ctx)
                test_acc = 0
                eval_time = 0

                print(num_epoch, " ", test_acc)
                with open(os.path.join(log_dir, "run.log"), "a") as fp:
                    fp.write("[Time %s][Epoch %d][Iteration %d] Test Acc %.5f\n"
                             % (time.time() - begin_time - eval_time,
                                num_epoch,
                                global_iters,
                                test_acc))

            if global_iters % num_batch_per_epoch == 0:
                num_epoch += 1
            global_iters += 1
