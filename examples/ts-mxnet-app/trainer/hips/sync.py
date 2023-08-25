import os
import time
import numpy as np
import mxnet as mx
from mxnet import kv, nd, autograd, gluon
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
    data_slice_idx = kwargs["data_slice_idx"]
    use_2bit_compression = kwargs["use_2bit_compression"]

    kvstore_dist = kv.create("dist_sync")
    is_master_worker = kvstore_dist.is_master_worker
    num_all_workers = kvstore_dist.num_all_workers
    rank = kvstore_dist.rank
    if is_master_worker:
        # Use with optimizer, update model on the global server
        kvstore_dist.set_optimizer(mx.optimizer.Adam(learning_rate=lr))
        if use_2bit_compression:
            # Use 2 bits compression
            kvstore_dist.set_gradient_compression({"type": "2bit", "threshold": 1e-6})
    # pause for a moment to complete the configuration
    if not os.path.exists(log_dir):
        os.mkdir(log_dir)
    if rank == 0:
        fp = open(os.path.join(log_dir, "run.log"), "w")
        fp.write("")
        fp.close()
    time.sleep(1)

    if is_master_worker and os.path.exists('net.params'):
            #net = gluon.SymbolBlock.imports('net-symbol.json', ['data'], 'net-0001.params')
            net.load_parameters("net.params")
            #params_save=np.load('param_save.npy')
            #params=params_save.tolist()
            with open(os.path.join(log_dir, "run.log"), "a") as fp:
                fp.write("Load net\n")
    params = list(net.collect_params().values())
    for idx, param in enumerate(params):
        if param.grad_req == "null":
            continue
        kvstore_dist.init(idx, param.data())
        if is_master_worker: continue
        kvstore_dist.pull(idx, param.data(), priority=-idx)

    #for idx, param in enumerate(params):
    #    if param.grad_req == "null":
    #        continue
     #   if is_master_worker:
     #       kvstore_dist.init(idx, param.data())
     #       continue
     #   else:
     #         kvstore_dist.pull(idx, param.data(), priority=-idx)

    nd.waitall()
    if is_master_worker: return

    train_iter, test_iter, num_train_samples, num_test_samples = load_data(
        batch_size,
        num_all_workers,
        data_slice_idx,
        data_type=data_type,
        resize=shape[-2:],
        root=data_dir
    )
    num_batch_per_epoch = np.ceil(num_train_samples / num_all_workers / batch_size)



    global_iters = 1
    num_epoch = 0
    begin_time = time.time()
    eval_time = 0

    conv_acc = 0;
    conv_step = 0;
    while num_epoch < num_total_epochs:
        for _, batch in enumerate(train_iter):
            Xs, ys, num_samples = get_batch(batch, ctx)
            ls = []
            with autograd.record():
                y_hats = [net(X) for X in Xs]
                ls = [loss(y_hat, y) for y_hat, y in zip(y_hats, ys)]
            for l in ls:
                l.backward()

            for idx, param in enumerate(params):
                if param.grad_req == "null":
                    continue
                kvstore_dist.push(idx, param.grad() / num_samples, priority=-idx)
                temp = nd.zeros(param.shape, ctx=ctx)
                kvstore_dist.pull(idx, temp, priority=-idx)
                temp.wait_to_read()
                param.set_data(temp)
            nd.waitall()

            interval = max(10 * num_epoch, 1)
            interval = min(interval, num_batch_per_epoch)
            if rank == 0 and global_iters % interval == 0:
                _ = time.time()
                test_acc = eval_acc(test_iter, net, ctx)
                nd.waitall()
                eval_time += (time.time() - _)
                with open(os.path.join(log_dir, "run.log"), "a") as fp:
                    if test_acc > conv_acc:
                        conv_acc = test_acc;
                        conv_step=0;

                    fp.write("[Time %s][Epoch %d][Iteration %d] Test Acc %.5f(%.5f)\n"
                             % (time.time() - begin_time - eval_time,
                                num_epoch,
                                global_iters,
                                test_acc,conv_acc))

                   # if  num_epoch > 5 and conv_step == 15:
                       # fp.write("Finished, Model has convergenced, conv-acc = %.5f"%(conv_acc))
                        #fp.flush();
                        #return;
                    					


            if global_iters % num_batch_per_epoch == 0:
                num_epoch += 1
                conv_step += 1
                net.save_parameters("net.params")
                #param_save=np.array(params)
                #np.save('param_save.npy',param_save)   # 保存为.npy格
           # if  conv_step == 5:
           #     fp.write("Finished, Model has convergenced, conv-acc = %.5f"%(conv_acc))
           #     fp.flush();
           #     return;

            global_iters += 1
