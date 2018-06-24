#!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI

def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class RingTopo(Topo):
    def build(self):
    	b=[0]*7
    	for i in range(1,7):
        	b[i] = self.addHost('b%d'%i)
        
        edges=[	(1,2),(2,3),(3,4),(4,5),(5,6),(6,1),\
        		(1,3),(3,5),(5,1),(2,4),(4,6),(6,2)]

        for (i,j) in edges:
        	self.addLink(b[i], b[j])
        
if __name__ == '__main__':
    topo = RingTopo()
    net = Mininet(topo = topo, controller = None) 

    # dic = { 'b1': 4, 'b2': 4, 'b3': 4, 'b4': 4,'b5':4,'b6':4 }
    nports = [4]*6

    for idx in range(len(nports)):
        name = 'b' + str(idx+1)
        node = net.get(name)
        clearIP(node)
        node.cmd('./disable_offloading.sh')
        node.cmd('./disable_ipv6.sh')

        # set mac address for each interface
        for port in range(nports[idx]):
            intf = '%s-eth%d' % (name, port)
            mac = '00:00:00:00:0%d:0%d' % (idx+1, port+1)

            node.setMAC(mac, intf = intf)

        node.cmd('./stp > %s-output.txt 2>&1 &' % name)

    net.start()
    CLI(net)
    net.stop()
