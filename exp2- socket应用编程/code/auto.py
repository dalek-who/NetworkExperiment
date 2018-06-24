from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI
import time
import os
import argparse

class MyTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        h3 = self.addHost('h3')
        s1 = self.addSwitch('s1')

        self.addLink(h1, s1)
        self.addLink(h2, s1)
        self.addLink(h3, s1)


#auto make
os.system('mn -c')

#arg options
parser = argparse.ArgumentParser()
parser.add_argument('-notmake',action='store_true',help='if -notmake, we do not makefile')
args = parser.parse_args()
if args.notmake==True:
    print('not make')
else:
    os.system('make clean')
    if os.system('make') != 0: #if success,return 0
        exit()
    print('Compile Success!!!')

print('preparing for mininet...')
topo = MyTopo()
net = Mininet(topo = topo)

print('net start')
net.start()
h1, h2, h3 = net.get('h1', 'h2', 'h3')

print('h2 running')
h2.cmd('./worker > h2.txt &')
time.sleep(3)
print('h3 running')
h3.cmd('./worker > h3.txt &')
time.sleep(3)
print('h1 running')
h1.cmd('./master war_and_peace.txt > h1.txt &')
time.sleep(10)
#CLI(net)
#net.stop()
print('net stop\n')

print('#####################  h1.txt  #####################')
os.system('cat h1.txt')
print('#####################  h2.txt  #####################')
os.system('cat h2.txt')
print('#####################  h3.txt  #####################')
os.system('cat h3.txt')

print('test finish')