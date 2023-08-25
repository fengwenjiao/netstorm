from mxnet.gluon.block import HybridBlock
from mxnet.gluon import nn
from mxnet import ndarray  as nd



# class GoogleNet(HybridBlock):
#     def __init__(self, classes=1000, **kwargs):
#         super(GoogleNet, self).__init__(**kwargs)
#         with self.name_scope():
#             # block 1
#             b1 = nn.Sequential()
#             b1.add(
#                 nn.Conv2D(64, kernel_size=7, strides=2,
#                           padding=3, activation='relu'),
#                 nn.MaxPool2D(pool_size=3, strides=2)
#             )
#             # block 2
#             b2 = nn.Sequential()
#             b2.add(
#                 nn.Conv2D(64, kernel_size=1),
#                 nn.Conv2D(192, kernel_size=3, padding=1),
#                 nn.MaxPool2D(pool_size=3, strides=2)
#             )
#
#             # block 3
#             b3 = nn.Sequential()
#             b3.add(
#                 Inception(64, 96, 128, 16, 32, 32),
#                 Inception(128, 128, 192, 32, 96, 64),
#                 nn.MaxPool2D(pool_size=3, strides=2)
#             )
#
#             # block 4
#             b4 = nn.Sequential()
#             b4.add(
#                 Inception(192, 96, 208, 16, 48, 64),
#                 Inception(160, 112, 224, 24, 64, 64),
#                 Inception(128, 128, 256, 24, 64, 64),
#                 Inception(112, 144, 288, 32, 64, 64),
#                 Inception(256, 160, 320, 32, 128, 128),
#                 nn.MaxPool2D(pool_size=3, strides=2)
#             )
#
#             # block 5
#             b5 = nn.Sequential()
#             b5.add(
#                 Inception(256, 160, 320, 32, 128, 128),
#                 Inception(384, 192, 384, 48, 128, 128),
#                 nn.AvgPool2D(pool_size=2)
#             )
#             # block 6
#             b6 = nn.Sequential()
#             b6.add(
#                 nn.Flatten(),
#                 nn.Dense(classes)
#             )
#             # chain blocks together
#             self.net = nn.HybridSequential()
#             self.net.add(b1, b2, b3, b4, b5, b6)
#
#         def hybrid_forward(self, F, x):
#             # out = x
#             # for i, b in enumerate(self.net):
#             #     out = b(out)
#                 # if self.verbose:
#                     # print('Block %d output: %s' % (i + 1, out.shape))
#             x=self.b6(x)
#             return x
#
# class Inception(nn.Block):
#     def __init__(self, n1_1, n2_1, n2_3, n3_1, n3_5, n4_1, **kwargs):
#         super(Inception, self).__init__(**kwargs)
#
#         # path1
#         self.p1_convs_1 = nn.Conv2D(n1_1, kernel_size=1, activation='relu')
#         # path2
#         self.p2_convs_1 = nn.Conv2D(n2_1, kernel_size=1, activation='relu')
#         # path2
#         self.p2_convs_3 = nn.Conv2D(n2_3, kernel_size=1, activation='relu')
#         # path3
#         self.p3_convs_1 = nn.Conv2D(n3_1, kernel_size=1, activation='relu')
#         self.p3_convs_5 = nn.Conv2D(n3_5, kernel_size=1, activation='relu')
#         # path4
#         self.p4_pool_3 = nn.MaxPool2D(pool_size=3, padding=1, strides=1)
#         self.p4_convs_1 = nn.Conv2D(n4_1, kernel_size=1, activation='relu')
#
#     def forward(self, x):
#         p1 = self.p1_convs_1(x)
#         p2 = self.p2_convs_3(self.p2_convs_1(x))
#         p3 = self.p3_convs_5(self.p3_convs_1(x))
#         p4 = self.p4_convs_1(self.p4_pool_3(x))
#         return nd.concat(p1, p2, p3, p4, dim=1)
#
#
#
# def googlenet(**kwargs):
#     net = GoogleNet(**kwargs)
#     return net

# def googlenet(**kwargs):
#
#     class GoogleNet(nn.Block):
#         def __init__(self, classes, verbose=False, **kwargs):
#             super(GoogleNet, self).__init__(**kwargs)
#             self.verbose = verbose
#             # add name_scope on the outer most Sequential
#             with self.name_scope():
#                 # block 1
#                 b1 = nn.Sequential()
#                 b1.add(
#                     nn.Conv2D(64, kernel_size=7, strides=2,
#                               padding=3, activation='relu'),
#                     nn.MaxPool2D(pool_size=3, strides=2)
#                 )
#                 # block 2
#                 b2 = nn.Sequential()
#                 b2.add(
#                     nn.Conv2D(64, kernel_size=1),
#                     nn.Conv2D(192, kernel_size=3, padding=1),
#                     nn.MaxPool2D(pool_size=3, strides=2)
#                 )
#
#                 # block 3
#                 b3 = nn.Sequential()
#                 b3.add(
#                     Inception(64, 96, 128, 16, 32, 32),
#                     Inception(128, 128, 192, 32, 96, 64),
#                     nn.MaxPool2D(pool_size=3, strides=2)
#                 )
#
#                 # block 4
#                 b4 = nn.Sequential()
#                 b4.add(
#                     Inception(192, 96, 208, 16, 48, 64),
#                     Inception(160, 112, 224, 24, 64, 64),
#                     Inception(128, 128, 256, 24, 64, 64),
#                     Inception(112, 144, 288, 32, 64, 64),
#                     Inception(256, 160, 320, 32, 128, 128),
#                     nn.MaxPool2D(pool_size=3, strides=2)
#                 )
#
#                 # block 5
#                 b5 = nn.Sequential()
#                 b5.add(
#                     Inception(256, 160, 320, 32, 128, 128),
#                     Inception(384, 192, 384, 48, 128, 128),
#                     nn.AvgPool2D(pool_size=2)
#                 )
#                 # block 6
#                 b6 = nn.Sequential()
#                 b6.add(
#                     nn.Flatten(),
#                     nn.Dense(classes)
#                 )
#                 # chain blocks together
#                 self.net = nn.Sequential()
#                 self.net.add(b1, b2, b3, b4, b5, b6)
#
#         def forward(self,  x, F=0):
#             # out = x
#             # for i, b in enumerate(self.net):
#             #     out = b(out)
#             #     if self.verbose:
#             #         print('Block %d output: %s' % (i + 1, out.shape))
#             # return out
#             x=self.net(x)
#             return x
#
#     class Inception(nn.Block):
#         def __init__(self, n1_1, n2_1, n2_3, n3_1, n3_5, n4_1, **kwargs):
#             super(Inception, self).__init__(**kwargs)
#
#             # path1
#             self.p1_convs_1 = nn.Conv2D(n1_1, kernel_size=1, activation='relu')
#             # path2
#             self.p2_convs_1 = nn.Conv2D(n2_1, kernel_size=1, activation='relu')
#             # path2
#             self.p2_convs_3 = nn.Conv2D(n2_3, kernel_size=1, activation='relu')
#             # path3
#             self.p3_convs_1 = nn.Conv2D(n3_1, kernel_size=1, activation='relu')
#             self.p3_convs_5 = nn.Conv2D(n3_5, kernel_size=1, activation='relu')
#             # path4
#             self.p4_pool_3 = nn.MaxPool2D(pool_size=3, padding=1, strides=1)
#             self.p4_convs_1 = nn.Conv2D(n4_1, kernel_size=1, activation='relu')
#
#         def forward(self, x):
#             p1 = self.p1_convs_1(x)
#             p2 = self.p2_convs_3(self.p2_convs_1(x))
#             p3 = self.p3_convs_5(self.p3_convs_1(x))
#             p4 = self.p4_convs_1(self.p4_pool_3(x))
#             return nd.concat(p1, p2, p3, p4, dim=1)
#
#     net = GoogleNet(**kwargs)
#     return net


#coding:utf-8
import mxnet as mx
from mxnet.gluon import nn
from mxnet import nd
import sys
import os
sys.path.append(os.getcwd())
import utils
from mxnet import gluon
from mxnet import init
class Inception(nn.Block):
    def __init__(self,n1_1,n2_1,n2_3,n3_1,n3_5,n4_1,**kwargs):
        super(Inception,self).__init__(**kwargs)

        # path 1
        self.p1_conv_1 = nn.Conv2D(n1_1,kernel_size = 1,
                                   activation='relu')

        # path 2
        self.p2_conv_1 = nn.Conv2D(n2_1,kernel_size = 1,
                                   activation='relu')
        self.p2_conv_3 = nn.Conv2D(n2_3,kernel_size=3,
                                   padding=1,activation='relu')
        # path 3
        self.p3_conv_1 = nn.Conv2D(n3_1,kernel_size=1,
                                   activation='relu')
        self.p3_conv_5 = nn.Conv2D(n3_5,kernel_size=5,padding=2,
                                   activation='relu')

        # path 4

        self.p4_pool_3 = nn.MaxPool2D(pool_size=3,padding=1,strides=1)
        self.p4_conv_1 = nn.Conv2D(n4_1,kernel_size=1,activation='relu')

    def forward(self,x):
        p1 = self.p1_conv_1(x)
        p2 = self.p2_conv_3(self.p2_conv_1(x))
        p3 = self.p3_conv_5(self.p3_conv_1(x))
        p4 = self.p4_conv_1(self.p4_pool_3(x))

        return nd.concat(p1,p2,p3,p4,dim=1)

incp = Inception(64,96,128,16,32,32)
incp.initialize()

x = nd.random.uniform(shape=(32,3,64,64))
result = incp(x)
print(result.shape)

class GoogLeNet(nn.Block):
    def __init__(self,num_classes,verbose=False,**kwargs):
        super(GoogLeNet,self).__init__(**kwargs)
        self.verbose = verbose
        with self.name_scope():
            # block 1
            b1 = nn.Sequential()
            b1.add(
                nn.Conv2D(64,kernel_size=7,strides=2,
                          padding=3,activation='relu'),
                nn.MaxPool2D(pool_size=3,strides=2)
            )

            # block 2
            b2 = nn.Sequential()
            b2.add(
                nn.Conv2D(64,kernel_size=1),
                nn.Conv2D(192,kernel_size=3,padding=1),
                nn.MaxPool2D(pool_size=3,strides=2)
            )

            # block 3
            b3 = nn.Sequential()
            b3.add(
                Inception(64,96,128,16,32,32),
                Inception(128,128,192,32,94,64),
                nn.MaxPool2D(pool_size=3,strides=2)
            )

            # block 4
            b4 = nn.Sequential()
            b4.add(
                Inception(192, 96, 208, 16, 48, 64),
                Inception(160, 112, 224, 24, 64, 64),
                Inception(128, 128, 256, 24, 64, 64),
                Inception(112, 144, 288, 32, 64, 64),
                Inception(256, 160, 320, 32, 128, 128),
                nn.MaxPool2D(pool_size=3, strides=2)
            )

            # block 5
            b5 = nn.Sequential()
            b5.add(
                Inception(256, 160, 320, 32, 128, 128),
                Inception(384, 192, 384, 48, 128, 128),
                nn.AvgPool2D(pool_size=2)
            )
            # block 6
            b6 = nn.Sequential()
            b6.add(
                nn.Flatten(),
                nn.Dense(num_classes)
            )
            # chain blocks together
            self.net = nn.Sequential()
            self.net.add(b1, b2, b3, b4, b5, b6)

    def forward(self,x):
        out = x
        for i ,b in enumerate(self.net):
            out = b(out)
            if self.verbose:
                print('Block %d output:%s' %(i + 1,out.shape))
        return out

net = GoogLeNet(10, verbose=True)
net.initialize()
x = nd.random.uniform(shape=(4, 3, 96, 96))
y = net(x)

train_data, test_data = utils.load_data_fashion_mnist(batch_size=64, resize=96)
ctx = mx.gpu(3)
net = GoogLeNet(10)
net.initialize(ctx=ctx, init=init.Xavier())
loss = gluon.loss.SoftmaxCrossEntropyLoss()
trainer = gluon.Trainer(net.collect_params(),'sgd', {'learning_rate': 0.01})
utils.train(train_data, test_data, net, loss,trainer, ctx, num_epochs=1)