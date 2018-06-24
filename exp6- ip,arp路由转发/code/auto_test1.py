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
        h3 = self.addHost('h3')
        r1 = self.addHost('r1')

        self.addLink(h1, r1)
        self.addLink(h2, r1)
        self.addLink(h3, r1)

if __name__ == '__main__':
    topo = RouterTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2, h3, r1 = net.get('h1', 'h2', 'h3', 'r1')
    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')
    h2.cmd('ifconfig h2-eth0 10.0.2.22/24')
    h3.cmd('ifconfig h3-eth0 10.0.3.33/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.2.1')
    h3.cmd('route add default gw 10.0.3.1')

    for h in (h1, h2, h3):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    r1.cmd('ifconfig r1-eth0 10.0.1.1/24')
    r1.cmd('ifconfig r1-eth1 10.0.2.1/24')
    r1.cmd('ifconfig r1-eth2 10.0.3.1/24')

    r1.cmd('./scripts/disable_arp.sh')
    r1.cmd('./scripts/disable_icmp.sh')
    r1.cmd('./scripts/disable_ip_forward.sh')

    os.system('make')
    r1.cmd('./router &')
    
    txt = 'test1.txt'
    os.system('echo %s > %s'%(txt,txt))
    os.system('echo >> %s'%txt)
    for ip in ('10.0.2.22','10.0.3.33','10.0.1.1','10.0.4.1','10.0.2.5'):
        print('ping %s -c 5 >> %s'%(ip,txt))
        os.system('echo >> %s'%txt)
        os.system('echo >> %s'%txt)
        os.system('echo >> %s'%txt)
        os.system('echo %s   h1 ping %s %s >> %s'%('\#'*20,ip,'\#'*20,txt))
        h1.cmd('ping %s -c 5 >> %s'%(ip,txt))
        sleep(5)

    #net.start()
    #CLI(net)
    #sleep(30)
    #net.stop()

    os.system('cat test1.txt')