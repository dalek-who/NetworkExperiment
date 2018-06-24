from threading import Thread
import time
import os

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI

class t_master(Thread):
    def __init__(self,node):
        super(t_master, self).__init__()
        self.node = node
    
    def run(self):
        print('xterm %s\n'%self.node)


class t_worker(Thread):
    def __init__(self,node):
        super(t_worker, self).__init__()
        self.node = node
    
    def run(self):
        print('xterm %s\n'%self.node)

class MyTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        h3 = self.addHost('h3')
        s1 = self.addSwitch('s1')

        self.addLink(h1, s1)
        self.addLink(h2, s1)
        self.addLink(h3, s1)

class t_main(Thread):
    def __init__(self,net):
        super(t_main, self).__init__()
        self.net = net

    def run(self):
        print('net start')
        net.start()
        CLI(net)



if __name__ == '__main__':

    #auto make
    os.system('mn -c')
    os.system('make clean')
    if os.system('make') != 0: #if success,return 0
        exit()
    print('Compile Success!!!')
    
    print('preparing for mininet...')
    topo = MyTopo()
    net = Mininet(topo = topo)
    
    master = t_master('h1')
    worker1 = t_worker('h2')
    worker2 = t_worker('h3')
    m = t_main(net)

    m.start()
    time.sleep(2)

    worker1.start()
    #worker2.start()
    time.sleep(2)
    
    #master.start()
    
    '''
    h1.cmdPrint('xterm')
    h2.cmdPrint('xterm')
    h3.cmdPrint('xterm')
    
    print('test running...')
    h2.cmdPrint('./worker')
    h3.cmdPrint('./worker')
    h1.cmdPrint('./master')
    '''
    net.stop()
