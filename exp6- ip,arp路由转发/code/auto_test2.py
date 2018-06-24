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
        
        r1 = self.addHost('r1')
        r2 = self.addHost('r2')
        r3 = self.addHost('r3')

        self.addLink(h1, r1)
        self.addLink(h2, r2)
        self.addLink(r1, r3)
        self.addLink(r2, r3)
                                 
'''                                                
    h1
    (10.0.1.11/24) h1-eth0
    |
    |
    (*10.0.1.1/24) r1-eth0
    r1
    (*10.0.3.1/24) r1-eth1
    |
    |
    (*10.0.3.2/24) r3-eth0
    r3
    (*10.0.4.1/24) r3-eth1
    |
    |
    (*10.0.4.2/24) r2-eth1
    r2
    (*10.0.2.1/24) r2-eth0
    |
    |
    (10.0.2.22/24) h2-eth0
    h2
'''
if __name__ == '__main__':
    topo = RouterTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2,  r1, r2, r3= net.get('h1', 'h2', 'r1','r2', 'r3')
    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')
    h2.cmd('ifconfig h2-eth0 10.0.2.22/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.2.1')

    for h in (h1, h2):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    r1.cmd('ifconfig r1-eth0 10.0.1.1/24')
    r1.cmd('ifconfig r1-eth1 10.0.3.1/24')
    
    r2.cmd('ifconfig r2-eth0 10.0.2.1/24')
    r2.cmd('ifconfig r2-eth1 10.0.4.2/24')

    r3.cmd('ifconfig r3-eth0 10.0.3.2/24')
    r3.cmd('ifconfig r3-eth1 10.0.4.1/24')

    for r in (r1, r2,r3):
        r.cmd('./scripts/disable_arp.sh')
        r.cmd('./scripts/disable_icmp.sh')
        r.cmd('./scripts/disable_ip_forward.sh')

    r1.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.2.0','10.0.3.2','r1-eth1'))   
    r1.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.4.0','10.0.3.2','r1-eth1'))   
    
    r2.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.1.0','10.0.4.1','r2-eth1'))
    r2.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.3.0','10.0.4.1','r2-eth1'))
    
    r3.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.1.0','10.0.3.1','r3-eth0'))   
    r3.cmd('route add -net %s netmask 255.255.255.0 gw %s dev %s'%('10.0.2.0','10.0.4.2','r3-eth1'))   
    
    for r in (r1, r2, r3):
   		r.cmd('./router &')

    txt = 'test2.txt'
    os.system('echo %s > %s'%(txt,txt))
    os.system('echo >> %s'%txt)
    
    print('traceroute 10.0.2.22 >> %s'%txt)
    os.system('echo %s traceroute 10.0.2.22 >> %s %s'%('\#'*20,txt,'\#'*20))
    h1.cmd('traceroute 10.0.2.22 >> %s'%txt)
    sleep(5)
    
    for ip in ('10.0.1.1','10.0.3.2','10.0.4.2','10.0.2.22'):
        print('ping %s -c 5 >> %s'%(ip,txt))
        os.system('echo >> %s'%txt)
        os.system('echo >> %s'%txt)
        os.system('echo >> %s'%txt)
        os.system('echo %s   h1 ping %s %s >> %s'%('\#'*20,ip,'\#'*20,txt))
        h1.cmd('ping %s -c 5 >> %s'%(ip,txt))
        #sleep(5)

    #net.start()
    #CLI(net)
    #net.stop()

    os.system('cat %s'%txt)
