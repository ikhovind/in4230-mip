#!/usr/bin/env python

import time
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.cli import CLI

class MyTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        
        self.addLink(h1, h2)


        
def run():
    topo = MyTopo()
    # No controller needed since there are no switches
    net = Mininet(topo=topo)
    net.start()
    h1, h2 = net.get('h1', 'h2')

    # Example command to verify the connection
    h1.cmd('cp ../ping_server /root/server')
    h1.cmd('cp ../mipd /root/mipd')

    h2.cmd('cp ../ping_client /root/client')
    h2.cmd('cp ../mipd /root/mipd')

    # Start the server on h1
    h1.cmd('/root/mipd h1_socket 100 > /tmp/mipd_h1.log 2>&1 &')
    #h1.cmd('/root/server h1_socket > /tmp/root_server.log 2>&1 &')

    time.sleep(1)
    #infot = h1.cmd("cat /tmp/root_server.log")
    infot = h1.cmd("cat /tmp/mipd_h1.log")
    info(infot)

    # Stop the network
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    run()
