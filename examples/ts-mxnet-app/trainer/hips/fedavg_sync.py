import os
import time
import json
import mxnet as mx
from mxnet import kv, nd, autograd
from utils import load_data, get_batch, eval_acc, Measure


def trainer(kwargs):
    lr = kwargs["lr"]
    batch_size = kwargs["batch_size"]
    num_local_epochs = kwargs["num_local_epochs"]
    data_dir = kwargs["data_dir"]
    data_type = kwargs["data_type"]
    log_dir = kwargs["log_dir"]
    eval_duration = kwargs["eval_duration"]
    ctx = kwargs["ctx"]
    shape = kwargs["shape"]
    net = kwargs["net"]
    loss = kwargs["loss"]
    trainer = kwargs["trainer"]
    split_by_class = kwargs["split_by_class"]
    data_slice_idx = kwargs["data_slice_idx"]
    use_2bit_compression = kwargs["use_2bit_compression"]
    global_iters = 1
    begin_time = 0

    kvstore_dist = kv.create("dist_sync")
    num_all_workers = kvstore_dist.num_all_workers
    is_master_worker = kvstore_dist.is_master_worker
    if is_master_worker:
        # Use with optimizer, update model on the global server
        kvstore_dist.set_optimizer(mx.optimizer.SGD(learning_rate=-1.0))
        # Use 2 bits compression
        if use_2bit_compression:
            kvstore_dist.set_gradient_compression({"type": "2bit", "threshold": 1e-6})
    # pause for a moment to complete the configuration
    time.sleep(1)

    params_file = os.path.join(log_dir, "net.params")
    meta_file = os.path.join(log_dir, "net.meta")
    if os.path.exists(params_file) and os.path.exists(meta_file):
        with open(meta_file, "r") as fp:
            meta = json.load(fp)
        global_iters = meta["global_iters"]
        begin_time = meta["begin_time"]
        if is_master_worker:
            net.load_parameters(params_file, ctx=ctx)

    params = list(net.collect_params().values())
    param2idx = {}
    original_params = {}
    for i, param in enumerate(params):
        param2idx[param.name] = i

    for param in params:
        if param.grad_req == "null":
            continue
        idx = param2idx[param.name]
        kvstore_dist.init(idx, param.data())
        kvstore_dist.pull(idx, param.data(), priority=-idx)
    nd.waitall()

    train_iter, test_iter = load_data(batch_size,
                                      num_all_workers,
                                      data_slice_idx,
                                      data_type=data_type,
                                      split_by_class=split_by_class,
                                      resize=shape[-2:],
                                      root=data_dir)

    if ctx == mx.cpu():
        subdir = "cpu"
    elif ctx == mx.gpu(0):
        subdir = "gpu0"
    elif ctx == mx.gpu(1):
        subdir = "gpu1"
    else:
        print("[ERROR] This gpu is not supported.")
        return

    measure = Measure(log_dir, subdir)
    measure.set_num_iters(global_iters)
    if begin_time:
        measure.set_begin_time(begin_time)

    while True:
        measure.next_iter()

        # back up original parameters
        for idx, param in enumerate(params):
            original_params[idx] = param.data().copy()

        # start local training
        measure.start("local training")
        for E in range(num_local_epochs):
            for _, batch in enumerate(train_iter):
                Xs, ys, num_samples = get_batch(batch, ctx)
                ls = []
                with autograd.record():
                    y_hats = [net(X) for X in Xs]
                    ls = [loss(y_hat, y) for y_hat, y in zip(y_hats, ys)]
                for l in ls:
                    l.backward()
                trainer.step(num_samples)
                nd.waitall()
                measure.add_samples(num_samples)
        measure.stop("local training")

        # start aggregating globally
        measure.start("global aggregation")
        for param in params:
            # # Use without optimizer, aggregate only
            # if param.grad_req == "null":
            #     # synchronize parameters in batch normalization layers
            #     if "gamma" in param.name or "beta" in param.name \
            #             or "running_mean" in param.name or "running_var" in param.name:
            #         idx = param2idx[param.name]
            #         kvstore_dist.push(idx, param.data() / num_all_workers, priority=-idx)
            #         agg_data = nd.zeros(param.shape, ctx=ctx)
            #         kvstore_dist.pull(idx, agg_data, priority=-idx)
            #         agg_data.wait_to_read()
            #         param.set_data(agg_data)
            # else:
            #     idx = param2idx[param.name]
            #     kvstore_dist.push(idx, (param.data() - original_params[idx]) / num_all_workers, priority=-idx)
            #     agg_grads = nd.zeros(param.shape, ctx=ctx)
            #     kvstore_dist.pull(idx, agg_grads, priority=-idx)
            #     agg_grads.wait_to_read()
            #     param_ = param.data() - lr * agg_grads
            #     param.set_data(param_)

            # Use with optimizer, aggregate and update model on the global server
            if param.grad_req == "null":
                continue
            idx = param2idx[param.name]
            kvstore_dist.push(idx, (param.data() - original_params[idx]) / num_all_workers, priority=-idx)
            temp = nd.zeros(param.shape, ctx=ctx)
            kvstore_dist.pull(idx, temp, priority=-idx)
            temp.wait_to_read()
            param.set_data(temp)
        nd.waitall()
        measure.stop("global aggregation")

        if kvstore_dist.rank == 0 and global_iters % eval_duration == 0:
            measure.start("evaluation")
            test_acc = eval_acc(test_iter, net, ctx)
            print('[Iteration %d] Test Acc %.3f' % (global_iters, test_acc))
            measure.set_accuracy(test_acc)
            measure.stop("evaluation")

            net.save_parameters(params_file)
            with open(meta_file, "w") as fp:
                json.dump({"global_iters": global_iters,
                           "test_acc": test_acc,
                           "learning_rate": lr,
                           "batch_size": batch_size,
                           "begin_time": measure.get_begin_time()}, fp)

        measure.save_report()

        global_iters += 1

        measure.reset(global_iters)

