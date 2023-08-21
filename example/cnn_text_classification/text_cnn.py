#!/usr/bin/env python

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# -*- coding: utf-8 -*-

"""Implementing CNN + Highway Network for Text Classification in MXNet"""

import os
import logging
import argparse
import numpy as np
import mxnet as mx
import data_helpers

logging.basicConfig(level=logging.DEBUG)

parser = argparse.ArgumentParser(description="CNN for text classification",
                                 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--pretrained-embedding', action='store_true',
                    help='use pre-trained word2vec only if specified')
parser.add_argument('--num-embed', type=int, default=300,
                    help='embedding layer size')
parser.add_argument('--gpus', type=str, default='',
                    help='list of gpus to run, e.g. 0 or 0,2,5. empty means using cpu. ')
parser.add_argument('--kv-store', type=str, default='local',
                    help='key-value store type')
parser.add_argument('--num-epochs', type=int, default=200,
                    help='max num of epochs')
parser.add_argument('--batch-size', type=int, default=50,
                    help='the batch size.')
parser.add_argument('--optimizer', type=str, default='rmsprop',
                    help='the optimizer type')
parser.add_argument('--lr', type=float, default=0.0005,
                    help='initial learning rate')
parser.add_argument('--dropout', type=float, default=0.0,
                    help='dropout rate')
parser.add_argument('--disp-batches', type=int, default=50,
                    help='show progress for every n batches')
parser.add_argument('--save-period', type=int, default=10,
                    help='save checkpoint for every n epochs')


def save_model():
    """Save cnn model

    Returns
    ----------
    callback: A callback function that can be passed as epoch_end_callback to fit
    """
    if not os.path.exists("checkpoint"):
        os.mkdir("checkpoint")
    return mx.callback.do_checkpoint("checkpoint/checkpoint", args.save_period)


def data_iter(batch_size, num_embed, pre_trained_word2vec=False):
    """Construct data iter

    Parameters
    ----------
    batch_size: int
    num_embed: int
    pre_trained_word2vec: boolean
                        identify the pre-trained layers or not
    Returns
    ----------
    train_set: DataIter
                Train DataIter
    valid: DataIter
                Valid DataIter
    sentences_size: int
                array dimensions
    embedded_size: int
                array dimensions
    vocab_size: int
                array dimensions
    """
    print('Loading data...')
    if pre_trained_word2vec:
        word2vec = data_helpers.load_pretrained_word2vec('data/rt.vec')
        x, y = data_helpers.load_data_with_word2vec(word2vec)
        # reshape for convolution input
        x = np.reshape(x, (x.shape[0], 1, x.shape[1], x.shape[2]))
        embedded_size = x.shape[-1]
        sentences_size = x.shape[2]
        vocabulary_size = -1
    else:
        x, y, vocab, vocab_inv = data_helpers.load_data()
        embedded_size = num_embed
        sentences_size = x.shape[1]
        vocabulary_size = len(vocab)

    # randomly shuffle data
    np.random.seed(10)
    shuffle_indices = np.random.permutation(np.arange(len(y)))
    x_shuffled = x[shuffle_indices]
    y_shuffled = y[shuffle_indices]

    # split train/valid set
    x_train, x_dev = x_shuffled[:-1000], x_shuffled[-1000:]
    y_train, y_dev = y_shuffled[:-1000], y_shuffled[-1000:]
    print('Train/Valid split: %d/%d' % (len(y_train), len(y_dev)))
    print('train shape:', x_train.shape)
    print('valid shape:', x_dev.shape)
    print('sentence max words', sentences_size)
    print('embedding size', embedded_size)
    print('vocab size', vocabulary_size)

    train_set = mx.io.NDArrayIter(
        x_train, y_train, batch_size, shuffle=True)
    valid = mx.io.NDArrayIter(
        x_dev, y_dev, batch_size)

    return train_set, valid, sentences_size, embedded_size, vocabulary_size


def sym_gen(batch_size, sentences_size, num_embed, vocabulary_size,
            num_label=2, filter_list=None, num_filter=100,
            dropout=0.0, pre_trained_word2vec=False):
    """Generate network symbol

    Parameters
    ----------
    batch_size: int
    sentences_size: int
    num_embed: int
    vocabulary_size: int
    num_label: int
    filter_list: list
    num_filter: int
    dropout: int
    pre_trained_word2vec: boolean
                        identify the pre-trained layers or not
    Returns
    ----------
    sm: symbol
    data: list of str
        data names
    softmax_label: list of str
        label names
    """
    input_x = mx.sym.Variable('data')
    input_y = mx.sym.Variable('softmax_label')

    # embedding layer
    if not pre_trained_word2vec:
        embed_layer = mx.sym.Embedding(data=input_x,
                                       input_dim=vocabulary_size,
                                       output_dim=num_embed,
                                       name='vocab_embed')
        conv_input = mx.sym.Reshape(data=embed_layer, target_shape=(batch_size, 1, sentences_size, num_embed))
    else:
        conv_input = input_x

    # create convolution + (max) pooling layer for each filter operation
    pooled_outputs = []
    for i, filter_size in enumerate(filter_list):
        convi = mx.sym.Convolution(data=conv_input, kernel=(filter_size, num_embed), num_filter=num_filter)
        relui = mx.sym.Activation(data=convi, act_type='relu')
        pooli = mx.sym.Pooling(data=relui, pool_type='max', kernel=(sentences_size - filter_size + 1, 1), stride=(1, 1))
        pooled_outputs.append(pooli)

    # combine all pooled outputs
    total_filters = num_filter * len(filter_list)
    concat = mx.sym.Concat(*pooled_outputs, dim=1)
    h_pool = mx.sym.Reshape(data=concat, target_shape=(batch_size, total_filters))

    # dropout layer
    if dropout > 0.0:
        h_drop = mx.sym.Dropout(data=h_pool, p=dropout)
    else:
        h_drop = h_pool

    # fully connected
    cls_weight = mx.sym.Variable('cls_weight')
    cls_bias = mx.sym.Variable('cls_bias')

    fc = mx.sym.FullyConnected(data=h_drop, weight=cls_weight, bias=cls_bias, num_hidden=num_label)

    # softmax output
    sm = mx.sym.SoftmaxOutput(data=fc, label=input_y, name='softmax')

    return sm, ('data',), ('softmax_label',)


def train(symbol_data, train_iterator, valid_iterator, data_column_names, target_names):
    """Train cnn model

    Parameters
    ----------
    symbol_data: symbol
    train_iterator: DataIter
                    Train DataIter
    valid_iterator: DataIter
                    Valid DataIter
    data_column_names: list of str
                       Defaults to ('data') for a typical model used in image classification
    target_names: list of str
                  Defaults to ('softmax_label') for a typical model used in image classification
    """
    devs = mx.cpu()  # default setting
    if args.gpus is not None:
        for i in args.gpus.split(','):
            mx.gpu(int(i))
        devs = mx.gpu()
    module = mx.mod.Module(symbol_data, data_names=data_column_names, label_names=target_names, context=devs)
    module.fit(train_data=train_iterator,
               eval_data=valid_iterator,
               eval_metric='acc',
               kvstore=args.kv_store,
               optimizer=args.optimizer,
               optimizer_params={'learning_rate': args.lr},
               initializer=mx.initializer.Uniform(0.1),
               num_epoch=args.num_epochs,
               batch_end_callback=mx.callback.Speedometer(args.batch_size, args.disp_batches),
               epoch_end_callback=save_model())


if __name__ == '__main__':
    # parse args
    args = parser.parse_args()

    # data iter
    train_iter, valid_iter, sentence_size, embed_size, vocab_size = data_iter(args.batch_size,
                                                                              args.num_embed,
                                                                              args.pretrained_embedding)
    # network symbol
    symbol, data_names, label_names = sym_gen(args.batch_size,
                                              sentence_size,
                                              embed_size,
                                              vocab_size,
                                              num_label=2,
                                              filter_list=[3, 4, 5],
                                              num_filter=100,
                                              dropout=args.dropout,
                                              pre_trained_word2vec=args.pretrained_embedding)
    # train cnn model
    train(symbol, train_iter, valid_iter, data_names, label_names)
