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
        h3 = self.addHost('h3')
        b1 = self.addHost('b1')

        self.addLink(h1, b1, bw=20)
        self.addLink(h2, b1, bw=10)
        self.addLink(h3, b1, bw=10)

if __name__ == '__main__':

    #arg options
    parser = argparse.ArgumentParser()
    parser.add_argument('-notmake',action='store_true',help='if -notmake, we do not makefile')
    parser.add_argument('-ref',action='store_true',help='if -ref, use hub-refeence')
    args = parser.parse_args()

    #clean net
    os.system('mn -c')
    
    #make
    if args.notmake==True:
        print('not make')
    else:
        os.system('make clean')
        if os.system('make') != 0: #if success,return 0
            exit()
        print('Compile Success!!!')
    
    run_file = 'hub'
    if args.ref == True:
        run_file = 'hub-reference'

    topo = BroadcastTopo()
    net = Mininet(topo = topo, link = TCLink, controller = None) 

    h1, h2, h3, b1 = net.get('h1', 'h2', 'h3', 'b1')
    h1.cmd('ifconfig h1-eth0 10.0.0.1/8')
    h2.cmd('ifconfig h2-eth0 10.0.0.2/8')
    h3.cmd('ifconfig h3-eth0 10.0.0.3/8')
    clearIP(b1)

    h1.cmd('./disable_offloading.sh')
    h2.cmd('./disable_offloading.sh')
    h3.cmd('./disable_offloading.sh')

    print('execute ./%s'%run_file)
    b1.cmd('./%s &'%run_file)

    def ping_text(h,ips,txt):
        h.cmd('echo ping %s -c 4 > %s &'%(ips[0],txt) )
        h.cmd('ping %s -c 4 >> %s &'%(ips[0],txt))
        h.cmd('echo ping %s -c 4 >> %s &'%(ips[1],txt) )
        h.cmd('ping %s -c 4 >> %s &'%(ips[1],txt))

        
    ip=('10.0.0.0','10.0.0.1','10.0.0.2','10.0.0.3')
    ping_text(h1,(ip[2], ip[3]), 'h1.txt')
    ping_text(h2,(ip[1], ip[3]), 'h2.txt')
    ping_text(h3,(ip[1], ip[2]), 'h3.txt')

    net.start()
    #CLI(net)
    time.sleep(10)
    net.stop()

    for h in ('h1','h2','h3'):
        print('\n'*3 +'#'*20 + '  %s.txt  '%h + '#'*20)
        os.system('cat %s.txt'%h)