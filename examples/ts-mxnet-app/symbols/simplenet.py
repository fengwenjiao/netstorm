from mxnet.gluon.block import HybridBlock
from mxnet.gluon import nn


class SimpleNet(HybridBlock):
    def __init__(self, classes=1000, **kwargs):
        super(SimpleNet, self).__init__(**kwargs)
        with self.name_scope():
            self.output = nn.Dense(classes)

    def hybrid_forward(self, F, x):
        x = self.output(x)
        return x


def simplenet(**kwargs):
    net = SimpleNet(**kwargs)
    return net
