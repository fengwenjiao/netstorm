#!/usr/bin/python2
import os
import sys
import time
import thread
import numpy as np
import random
import threading
import random
import copy


h1 = "6285e039620b5d40" # change to netstorm-trainer1-ID
h2 = "6285e039620b5d41" # change to netstorm-trainer2-ID
h3 = "6285e039620b5d42" # change to netstorm-trainer3-ID
h4 = "6285e039620b5d43" # change to netstorm-trainer4-ID
h5 = "6285e039620b5d44" # change to netstorm-trainer5-ID
h6 = "6285e039620b5d45" # change to netstorm-trainer6-ID
h7 = "6285e039620b5d46" # change to netstorm-trainer7-ID
h8 = "6285e039620b5d47" # change to netstorm-trainer8-ID
h9 = "6285e039620b5d48" # change to netstorm-trainer9-ID
h10 = "6285e039620b5d49" # change to netstorm-trainer10-ID
h11 = "6285e039620b5d4a" # change to netstorm-trainer11-ID


min_=100.
max_=1000.
configs1=[[h1,"toh3",500.,"2"],[h1,"toh4",100.,"3"],
          [h2,"toh3",500.,"3"],[h2,"toh4",500.,"1"],[h2,"toh5",500.,"5"],
          [h3,"toh3",100.,"1"],[h3,"toh4",500.,"2"],[h3,"toh5",100.,"4"],
          [h4,"toh3",100.,"3"],[h4,"toh4",500.,"5"],[h4,"toh5",100.,"6"],
          [h5,"toh3",500.,"2"],[h5,"toh4",500.,"7"],[h5,"toh5",500.,"4"],
          [h6,"toh3",100.,"4"],[h6,"toh4",500.,"7"],[h6,"toh5",100.,"8"],[h6,"toh6",100.,"9"],
          [h7,"toh3",500.,"5"],[h7,"toh5",500.,"6"],[h7,"toh6",500.,"8"],
          [h8,"toh3",100.,"6"],[h8,"toh4",500.,"7"],[h8,"toh5",500.,"9"],
          [h9,"toh3",100.,"6"],[h9,"toh4",500.,"8"],
          ]

def set_net():
    count=1
    seed=1
    random.seed(seed)
    for config in configs1:
        config[2]=150.
    while 1:
        print(count)
        count=count+1
        for i,config in enumerate(configs1):
            cmd = "sudo docker exec -it "+config[0]+" /bin/bash -c "
            #reset
            reset_tc =  "\"tc qdisc del dev "+config[1]+" root 2> /dev/null > /dev/null\""
            os.system(cmd+reset_tc)
            #egress
            set_root = "\"tc qdisc add dev "+config[1]+" root handle 1: htb default 2\""
            os.system(cmd+set_root)
            config[2]=random.randint(20,155)
            print int(config[2]) ,
            root_class0 = "\"tc class add dev "+config[1]+" parent 1:1 classid 1:2 htb rate "+str(int(config[2]))+"mbit prio 2\""

            os.system(cmd+root_class0)
            class1_qdisc1 = "\"tc qdisc add dev "+config[1]+" parent 1:2 sfq perturb 10\""
            os.system(cmd+class1_qdisc1)
        print "||"
        print("thread keep waitting!")

        time.sleep(180)


if __name__=="__main__":
    p=threading.Thread(target=set_net,args=())
    p.start()
    p.join()
