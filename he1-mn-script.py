#!/usr/bin/env python

""" mininet script to test IN3230/IN4230 HE1 assignments"""

from mininet.topo import Topo

from mininet.cli import CLI
# from mininet.term import makeTerm
from mininet.term import tunnelX11
import os
import signal
import time

# Usage example:
# sudo -E mn --mac --custom he1-mn-script.py --topo he1 --link tc


class He1Topo(Topo):
    "Simple topology for Home Exam 1."

    def __init__(self):
        "Set up our custom topo."

        # Initialize topology
        Topo.__init__(self)

        # Create 3 hosts, A, B, and C..
        A = self.addHost('A')
        B = self.addHost('B')
        C = self.addHost('C')

        # Create p2p links between A - B and B - C.
        self.addLink(A, B, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, C, bw=10, delay='10ms', loss=0.0, use_tbf=False)


terms = []


def openTerm(self, node, title, geometry, cmd="bash"):
    "Open xterm window."

    display, tunnel = tunnelX11(node)

    return node.popen(["xterm",
                       "-hold",
                       "-title", "'%s'" % title,
                       "-geometry", geometry,
                       "-display", display,
                       "-e", cmd])


def init_he1(self, line):
    "init is an example command to extend the Mininet CLI"

    net = self.mn
    A = net.get('A')
    B = net.get('B')
    C = net.get('C')

    # Start MIP daemons
    terms.append(openTerm(self,
                          node=A,
                          title="Host A",
                          geometry="80x20+0+0",
                          cmd="./mipd -d usockA 10"))
    terms.append(openTerm(self,
                          node=B,
                          title="Host B",
                          geometry="80x20+550+0",
                          cmd="./mipd -d usockB 20"))
    terms.append(openTerm(self,
                          node=C,
                          title="Host C",
                          geometry="80x20+1100+0",
                          cmd="./mipd -d usockC 30"))

    # Don't launch applications right away
    time.sleep(1)

    # Run ping_server on Host B
    terms.append(openTerm(self,
                          node=B,
                          title="Server [B]",
                          geometry="80x20+550+300",
                          cmd="./ping_server usockB"))

    time.sleep(1)

    # Run ping_clients on Hosts A and C
    terms.append(openTerm(self,
                          node=A,
                          title="Client [A]",
                          geometry="80x20+0+300",
                          cmd="./ping_client usockA 20 \"Hello IN3230\""))

    terms.append(openTerm(self,
                          node=C,
                          title="Client [C]",
                          geometry="80x20+0+600",
                          cmd="./ping_client usockC 20 \"Hello IN4230\""))

    # This MUST output 'ping timeout' since A is not able to reach C.
    terms.append(openTerm(self,
                          node=A,
                          title="Client [A]",
                          geometry="80x20+0+300",
                          cmd="./ping_client usockA 30 \"Hello IN4230\""))
    #time.sleep(1)

    # This time the RTT should be smaller since MIP-ARP cache is being used.
    terms.append(openTerm(self,
                          node=A,
                          title="Client [A]",
                          geometry="80x20+0+300",
                          cmd="./ping_client usockA 20 \"Hello again IN4230\""))


# Mininet Callbacks
# Inside mininet console run 'init_he1'


CLI.do_init_he1 = init_he1


# Inside mininet console run 'EOF' to gracefully kill the mininet console
orig_EOF = CLI.do_EOF


# Kill mininet console
def do_EOF(self, line):
    for t in terms:
        os.kill(t.pid, signal.SIGKILL)
    return orig_EOF(self, line)


CLI.do_EOF = do_EOF


# Topologies
topos = {
    'he1': (lambda: He1Topo()),
}
