from mininet.topo import Topo
from mininet.node import Node
class fattopo(Topo):
	numHosts = 16
	numTor = 8
	numAgg = 8
	numCore = 4
	#hostIPset = '9.0.0.'
	#hostMask = 27

	def createHosts(self, numHosts):
		for num in range(1, numHosts+1):
			currName = 'h' + str(num)
			self.addHost(currName)
		#self.setHostIPs()
		return self.hosts()

	#def setHostIPs(self):		
			#hosts = self.hosts()
			#host = self.fdToNode('h1')
			#print type(host)
			#hostList[num-1].setIP(self.hostIPset, self.hostMask)
			
	def createSwitches(self, numSwitches, nextSwitch):
		layer = []
		for num in range(nextSwitch, numSwitches + nextSwitch):
			currName = 's' + str(num)
			layer.append(self.addSwitch(currName))
		return layer	

	def linkHostsTor(self, hosts, tor):
		torNum = 0
		for hostNum in range(len(hosts)): #1 connection per host
			if hostNum % 2 == 0:
				torNum = hostNum / 2
			else:
				torNum = int(hostNum / 2)
			self.addLink(hosts[hostNum],tor[torNum])

	def linkTorAgg(self, tor, agg):
		firstPodSwitch = True
		for i in range(2 * len(tor)): #2 connections per ToR switch
			HI = i / 2
			#need two conditions; need to keep track of two layers
			if i % 2 == 0 and firstPodSwitch == True:
				torNum, aggNum = HI, HI
			elif firstPodSwitch == True:
				torNum = int(HI)
				aggNum = int(HI) + 1
				firstPodSwitch = False
			elif i % 2 == 0:
				torNum = HI
				aggNum = HI - 1
			else:
				torNum = (i - 1) / 2 
				aggNum = (i - 1) / 2
				firstPodSwitch = True
			self.addLink(tor[torNum], agg[aggNum])

	def linkAggCore(self, agg, core):
		for i in range(len(agg)):
			if i % 2 == 0:
				self.addLink(agg[i], core[0])
				self.addLink(agg[i], core[1])
			else:
				self.addLink(agg[i], core[2])		
				self.addLink(agg[i], core[3])

	def printAddrs(self):
		for curr in self.hosts():
			print 'host: ' + str(curr) + ', IP: ' #+ host.IP
	
	def __init__(self):
		firstSwitchNum = 1
		Topo.__init__(self)
		hosts = self.createHosts(self.numHosts)
		tor = self.createSwitches(self.numTor, firstSwitchNum)
		agg = self.createSwitches(self.numAgg, len(tor) + firstSwitchNum)
		core = self.createSwitches(self.numCore, len (tor) + len(agg) + firstSwitchNum)
		self.linkHostsTor(hosts, tor)
		self.linkTorAgg(tor, agg)
		self.linkAggCore(agg, core)
		#self.printAddrs()

topos = {'fattopo': (lambda: fattopo()) }

