#!/usr/bin/python

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI
import os

class MyTopo(Topo):
    "Topology connected by routers"
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        h3 = self.addHost('h3')
        s1 = self.addSwitch('s1')

        self.addLink(h1, s1)
        self.addLink(h2, s1)
        self.addLink(h3, s1)

os.system("make")
topo = MyTopo()
net = Mininet(topo = topo)

net.start()

h1, h2, h3 = net.get('h1', 'h2', 'h3')
h2.cmd('./worker &')
h3.cmd('./worker &')
print h1.cmd('./master war_and_peace.txt')

net.stop()

os.system("make clean")