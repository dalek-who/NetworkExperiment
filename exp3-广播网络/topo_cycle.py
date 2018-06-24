#!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.cli import CLI

import os
import time
import argparse

# Mininet will assign an IP address for each interface of a node 
# automatically, but hub or switch does not need IP address.
def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class BroadcastTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        
        b1 = self.addHost('b1')
        b2 = self.addHost('b2')
        b3 = self.addHost('b3')

        self.addLink(h1, b1)
        self.addLink(h2, b2)
        
        self.addLink(b1, b2)
        self.addLink(b1, b3)
        self.addLink(b2, b3)

if __name__ == '__main__':

    #arg options
    parser = argparse.ArgumentParser()
    parser.add_argument('-notmake',action='store_true',help='if -notmake, we do not makefile')
    parser.add_argument('-ref',action='store_true',help='if -ref, use hub-refeence')
    args = parser.parse_args()

    #clean net
    os.system('mn -c')
    os.system('make clean')

    #make
    if args.notmake==True:
        print('not make')
    else:
        if os.system('make') != 0: #if success,return 0
            exit()
        print('Compile Success!!!')
    
    run_file = 'hub'
    if args.ref == True:
        run_file = 'hub-reference'

    topo = BroadcastTopo()
    net = Mininet(topo = topo, link = TCLink, controller = None) 

    h1, h2, b1, b2, b3 = net.get('h1', 'h2', 'b1','b2','b3')
    h1.cmd('ifconfig h1-eth0 10.0.0.1/8')
    h2.cmd('ifconfig h2-eth0 10.0.0.2/8')
    clearIP(b1)
    clearIP(b2)
    clearIP(b3)

    h1.cmd('./disable_offloading.sh')
    h2.cmd('./disable_offloading.sh')

    print('execute ./%s'%run_file)
    b1.cmd('./%s &'%run_file)
    b2.cmd('./%s &'%run_file)
    b3.cmd('./%s &'%run_file)

    net.start()
    CLI(net)
    net.stop()