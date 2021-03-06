#!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI
import os
from time import sleep

class RouterTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        n1 = self.addHost('n1')

        self.addLink(h1, n1)
        self.addLink(h2, n1)

if __name__ == '__main__':
    topo = RouterTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2, n1 = net.get('h1', 'h2', 'n1')

    h1.cmd('ifconfig h1-eth0 10.21.0.1/16')
    h1.cmd('route add default gw 10.21.0.254')

    n1.cmd('ifconfig n1-eth0 10.21.0.254/16')
    n1.cmd('ifconfig n1-eth1 159.226.39.43/24')

    h2.cmd('ifconfig h2-eth0 159.226.39.123/24')

    for h in (h1, h2):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    n1.cmd('./scripts/disable_arp.sh')
    n1.cmd('./scripts/disable_icmp.sh')
    n1.cmd('./scripts/disable_ip_forward.sh')

    os.system('make')
    
    os.system('echo n1: ./nat &')
    n1.cmd('./nat &')

    os.system('echo h2: ./scripts/disable_offloading.sh &')
    h2.cmd('./scripts/disable_offloading.sh &')
    os.system('echo h2: python -m SimpleHTTPServer &')
    h2.cmd('python -m SimpleHTTPServer &')

    os.system('echo h1: ./scripts/disable_offloading.sh')
    h1.cmd('./scripts/disable_offloading.sh')
    os.system('echo h1: wget http://159.226.39.123:8000')
    h1.cmd('wget http://159.226.39.123:8000  >> wget.txt')
    
    os.system('cat wget.txt')
    net.start()
#    CLI(net)
    net.stop()
