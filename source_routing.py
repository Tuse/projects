from ryu.base import app_manager
from ryu.controller import ofp_event, dpset
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.controller.handler import set_ev_cls

debug = True

class Controller(app_manager.RyuApp):
    def __init__(self, *args, **kwargs):
        super(Controller, self).__init__(*args, **kwargs)
            
    @set_ev_cls(dpset.EventDP, MAIN_DISPATCHER)
    def switch_in(self, ev):
        dp = ev.dp
        entered = ev.enter
        if ev.enter:
            self.install_rules(dp)

    def install_rules(self, dp):
        ofp = dp.ofproto
        ofp_parser = dp.ofproto_parser

        # Make sure the switch's forwarding table is empty
        dp.send_delete_all_flows()

        # Creates a rule that sends out packets coming
        # from port: inport to the port: outport
        def from_port_to_port(inport, outport, vlan):
            #if debug == True:
                #print "sID:" + str(dp.id) + " vlan=" + str(vlan) + ", inport=" + str(inport) + ", outport=" + str(outport)
            match = ofp_parser.OFPMatch(in_port=inport, dl_vlan=vlan)
            actions = [ofp_parser.OFPActionOutput(outport)]
            out = ofp_parser.OFPFlowMod(datapath=dp, cookie=0, command=ofp.OFPFC_ADD, match=match, actions=actions)
            dp.send_msg(out)
            
        base_vlans = [0, 1, 2, 3]
        #vlan: 10 bits --bbbbssssvv
        base_vl_mask = 0b0000000011 #last two bits
        bg_host_mask = 0b1111000000
        sm_host_mask = 0b0000111100
        switch = dp.id
    
        def get_vlan(base, h1, h2):
            vlan = base
            big = max(h1, h2)
            small = min(h1, h2)
            big_shift = 6
            small_shift = 2
            vlan = vlan | (big << big_shift)
            vlan = vlan | (small << small_shift)
            if debug == True:
                print "sID:%d h1=%d, h2=%d, vlan=%d, base=%d" % (dp.id, h1, h2, vlan, base)
            return vlan
            
        def get_other_pod_switch(switch):#(in same layer)
            other_pod_switch = switch + 1 #2 switches in every pod
            if switch % 2 == 0:
                other_pod_switch = switch - 1
            return other_pod_switch
            
        def get_switch_hosts(switch):
            bg_host = switch * 2
            sm_host = bg_host - 1
            switch_hosts = [sm_host, bg_host]
            return switch_hosts
        
        def get_outside_pod_switches(switch):
            num_switches = 8
            other_pod_switch = get_other_pod_switch(switch)
            switches = []
            for outside_pod_switch in range(1, num_switches+1):
                if outside_pod_switch != switch and outside_pod_switch != other_pod_switch:
                    switches.append(outside_pod_switch)
            return switches
        
        def get_switch_host_by_port(switch, port):
            sm_port = 1
            host = switch * 2
            if port == sm_port:
                 host = host - 1
            return host
            
        def get_switch_port_by_host(host):
            port = 1
            if host % 2 == 0:
                port = 2
            return port
        
        #routing rules based port of incoming traffic
        def install_tor_rules():
            #ports 1,2: connected to hosts
            #ports 3,4: connected to agg layer switches
            host_ports = [1, 2]
            agg_ports = [3, 4]
            
            #case 1: inport=1 or 2, destined for neighbor; route down on other down port
            def install_case_1():
                if debug == True:
                    print "tor,case1"
                for inport in host_ports:
                    #only one possible vlan
                    switch_hosts = get_switch_hosts(switch)
                    vlan = 0
                    vlan = vlan | (max(switch_hosts) << 6)
                    vlan = vlan | (min(switch_hosts) << 2)

                    #outport is simply the other port
                    outport = min(host_ports)
                    if inport == outport:
                        outport = max(host_ports)

                    from_port_to_port(inport, outport, vlan)
                        
            #case 2: inport=1 or 2, destined for host in same pod; route up according to base vlan
            def install_case_2():
                if debug == True:
                    print "tor,case2"
                for inport in host_ports:

                    src = get_switch_host_by_port(switch, inport)
                    other_pod_switch = get_other_pod_switch(switch)
                    other_switch_hosts = get_switch_hosts(other_pod_switch)

                    #case 2a,b: base vlan = 0 (agg switch directly above) OR base vlan = 1 (opposite agg switch)
                    base_vlans = [0, 1]
                    for vlan in base_vlans:
                        if (switch % 2) == 0: #rightmost Tor switch in pod
                            if vlan == min(base_vlans): #vlan=0
                                outport = max(agg_ports) #outport=4
                            else:
                                outport = min(agg_ports) #outport=3#leftmost Tor switch in pod

                        else: #leftmost Tor switch in pod
                            if vlan == min(base_vlans): #vlan=0
                                outport = min(agg_ports) #outport=3
                            else:
                                outport = max(agg_ports) #outport=4

                        possible_vlans = []
                        for dst in other_switch_hosts:
                            possible_vlans.append(get_vlan(vlan, src, dst))

                        for vlan in possible_vlans:
                            from_port_to_port(inport, outport, vlan)
                        
            #case 3: inport=1 or 2, destined for core; route up according to base vlan
            def install_case_3():
                if debug == True:
                    print "tor,case3"
                outside_pod_switches = get_outside_pod_switches(switch)
                outside_pod_hosts = [] #all hosts in all other pods
                for outside_pod_switch in outside_pod_switches:
                    outside_pod_hosts.extend(get_switch_hosts(outside_pod_switch))

                if switch % 2 != 0: #case 3a: leftmost agg switch
                    #case 3a.i: base vlan is 0 or 1 - route up, outport = 3
                    base_vlans = [0, 1] 
                    outport = 3
                    for inport in host_ports:

                        src = get_switch_host_by_port(switch, inport)

                        for vlan in base_vlans:
                            possible_vlans = []
                            for dst in outside_pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                            for vlan in possible_vlans:
                                from_port_to_port(inport, outport, vlan)
                                
                    #case 3a.ii: base vlan is 2 or 3 - route across, outport = 4
                    base_vlans = [2, 3]
                    outport = 4
                    for inport in host_ports:

                        src = get_switch_host_by_port(switch, inport)

                        for vlan in base_vlans:
                            possible_vlans = []
                            for dst in outside_pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                            for vlan in possible_vlans:
                                from_port_to_port(inport, outport, vlan)
                            
                else: #case 3b: rightmost agg switch
                    #case 3b.i: base vlan is 0 or 1 - route across, outport = 4
                    base_vlans = [0, 1] 
                    outport = 3
                    for inport in host_ports:

                        src = get_switch_host_by_port(switch, inport)

                        for vlan in base_vlans:
                            possible_vlans = []
                            for dst in outside_pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                            for vlan in possible_vlans:
                                from_port_to_port(inport, outport, vlan)
                                
                    #case 3b.ii: base vlan is 2 or 3 - route up, outport = 3
                    base_vlans = [2, 3]
                    outport = 4
                    for inport in host_ports:

                        src = get_switch_host_by_port(switch, inport)

                        for vlan in base_vlans:
                            possible_vlans = []
                            for dst in outside_pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                            for vlan in possible_vlans:
                                from_port_to_port(inport, outport, vlan)
                        
                        
            #case 4: inport=3 or 4, destined for attached host; route down according to base vlan
            #one, and only one, of the hosts coded within the vlan will be a host attached to the switch
            #only one of the hosts can be contained b/c the other attached host cannot be routed from port 3 or 4
            def install_case_4():
                if debug == True:
                    print "tor,case4"
                outside_pod_switches = get_outside_pod_switches(switch)
                possible_srcs = [] #all hosts attached to all other switches
                for outside_pod_switch in outside_pod_switches:
                    possible_srcs.extend(get_switch_hosts(outside_pod_switch))
                other_pod_switch = get_other_pod_switch(switch)
                possible_srcs.extend(get_switch_hosts(other_pod_switch))
                
                for inport in agg_ports:

                    connected_hosts = get_switch_hosts(switch)

                    for dst in connected_hosts:
                        outport = get_switch_port_by_host(dst)

                        base_vlans = [0, 1, 2, 3]
                        for vlan in base_vlans:

                            possible_vlans = []
                            for src in possible_srcs:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                            for vlan in possible_vlans:
                                from_port_to_port(inport, outport, vlan)
                                
            install_case_1()
            install_case_2()
            install_case_3()
            install_case_4()
                        
        def install_agg_rules():
            #ports 1,2: connected to ToR switches
            #ports 3,4: connected to core switches
            tor_ports = [1, 2]
            core_ports = [3, 4]
            num_tor_switches = 8

            #case 1: traffic coming in on ports 1 or 2 destined for other pod - route to core switch based on vlan
            def install_case_1():
                
                #case Ia: first agg switch in pod (odd sID); only deal with base vlans 0 and 1
                if switch % 2 != 0:
                    #each agg switch is attached to two core switches, the larger ID of which is even
                    if debug == True:
                        print "agg,case1a; left agg switch, inport=1,2, dst=other pod, outport=3,4; vlans=0,1"
                    
                    base_vlans = [0, 1]

                    possible_vlans = []
                    for vlan in base_vlans:

                        #all hosts in pod
                        pod_hosts = []
                        other_pod_switch = get_other_pod_switch(switch)
                        pod_hosts.extend(get_switch_hosts(switch - num_tor_switches))
                        pod_hosts.extend(get_switch_hosts(other_pod_switch - num_tor_switches))
                        if debug == True:
                            print "agg rules case I: podhosts=%s" % pod_hosts

                        #all hosts from other pods (possible destinations)vlans for all hosts in pod
                        outside_pod_switches = get_outside_pod_switches(switch)
                        outside_pod_hosts = [] #all hosts in all other pods
                        for outside_pod_switch in outside_pod_switches:
                            outside_pod_hosts.extend(get_switch_hosts(outside_pod_switch))

                        for dst in outside_pod_hosts:
                            for src in pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                    for inport in tor_ports: #incoming traffic port

                        for vlan in possible_vlans:
                            outport = 3 #if base vlan = min of the two base vlans, route via link 3, otherwise 4
                            if (vlan & base_vl_mask) == 1:
                                outport = 4
                            from_port_to_port(inport, outport, vlan)
                        
                #case Ib: second agg switch in pod (even sID); only deal with base vlans 2 and 3
                if switch % 2 == 0:
                    #each agg switch is attached to two core switches, the larger ID of which is even
                    if debug == True:
                        print "agg,case1b; right agg switch, inport=1,2, dst=other pod, outport=3,4; vlans=2,3"
                    
                    base_vlans = [2, 3]

                    possible_vlans = []
                    for vlan in base_vlans:

                        #all hosts in pod
                        pod_hosts = []
                        other_pod_switch = get_other_pod_switch(switch)
                        pod_hosts.extend(get_switch_hosts(switch - num_tor_switches))
                        pod_hosts.extend(get_switch_hosts(other_pod_switch - num_tor_switches))

                        if debug == True:
                            print "agg rules case II: podhosts=%s" % pod_hosts

                        #all hosts from other pods (possible destinations)vlans for all hosts in pod
                        outside_pod_switches = get_outside_pod_switches(switch)
                        outside_pod_hosts = [] #all hosts in all other pods
                        for outside_pod_switch in outside_pod_switches:
                            outside_pod_hosts.extend(get_switch_hosts(outside_pod_switch))

                        for dst in outside_pod_hosts:
                            for src in pod_hosts:
                                possible_vlans.append(get_vlan(vlan, src, dst))

                    for inport in tor_ports: #incoming traffic port

                        for vlan in possible_vlans:
                            outport = 3 #if base vlan = min of the two base vlans, route via link 3, otherwise 4
                            if (vlan & base_vl_mask) == 3:
                                outport = 4
                            from_port_to_port(inport, outport, vlan)
                
            #case 2: traffic coming in on ports 1 or 2 destined for same pod - route to ToR switch based on vlan
            def install_case_2():
                if debug == True:
                    print "agg,case2; inport=1,2, dst=same pod, route to ToR"
                
                base_vlans = [0, 1] #only two paths between hosts in same pod on different ToR swtiches
                    
                #other_switch = get_other_pod_switch(switch)
                #other_pod_hosts = get_switch_hosts(other_switch)
                #again, first agg switch in pod (odd sID); only deal with base vlans 0 and 1
                
                possible_vlans = []
                for vlan in base_vlans:

                    #hosts on first ToR switch in pod
                    first_tor_switch = switch - num_tor_switches
                    tor_1_hosts = []
                    tor_1_hosts.extend(get_switch_hosts(first_tor_switch)) #tor switch directly below current agg switch
                    
                    #hosts on second ToR switch in pod
                    second_tor_switch = get_other_pod_switch(first_tor_switch)
                    tor_2_hosts = []
                    tor_2_hosts.extend(get_switch_hosts(second_tor_switch))
                    
                    for src in tor_1_hosts:
                        for dst in tor_2_hosts:
                            possible_vlans.append(get_vlan(vlan, src, dst))
                
                for inport in tor_ports: #incoming traffic port
                    outport = max(tor_ports)
                    if outport == inport:
                        outport = min(tor_ports)

                    for vlan in possible_vlans:
                        from_port_to_port(inport, outport, vlan)
                
                    
            #case 3: traffic coming in on ports 3 or 4 - route to appropriate ToR switch based on vlan
            def install_case_3():
                
                #case 3a: dst on first ToR switch - outport = 1
                if debug == True:
                    print "\nagg,case3a, inport=3,4; dst=left tor switch, outport=1\n"
                
                outport = 1
                #vlans destined for the left tor switch go out on 1 regardless of the agg switch
                left_tor_vlans = [] #vlans destined for the left tor switch
                
                #get hosts from left tor switch and the vlans we will be dealing with (only two depending on agg switch)
                left_tor_hosts = []
                left_tor_switch = switch - num_tor_switches
                base_vlans = [0, 1] #leftmost agg switch only connected to first two core switches
                if switch % 2 == 0:
                    left_tor_switch = left_tor_switch - 1
                    base_vlans = [2, 3] #rightmost agg switch connected to second two core switches
                left_tor_hosts.extend(get_switch_hosts(left_tor_switch))
                    
                #all hosts from other pods (possible sources)
                outside_pod_switches = get_outside_pod_switches(switch)
                outside_pod_hosts = [] #all hosts in all other pods
                for outside_pod_switch in outside_pod_switches:
                    outside_pod_hosts.extend(get_switch_hosts(outside_pod_switch))
                    
                for dst in left_tor_hosts:
                    for src in outside_pod_hosts:
                        for vlan in base_vlans:
                            left_tor_vlans.append(get_vlan(vlan, src, dst))
                            
                for inport in core_ports: #incoming traffic port
                    for vlan in left_tor_vlans:
                        from_port_to_port(inport, outport, vlan)
                        
                        
                #case 3b: dst on second (right) ToR switch - outport = 2
                if debug == True:
                    print "\nagg,case3b, inport=3,4; dst=right tor switch, outport=2\n"
                
                outport = 2
                right_tor_vlans = [] #vlans destined for the left tor switch
                
                #get hosts from left tor switch and the vlans we will be dealing with (only two depending on agg switch)
                right_tor_hosts = []
                right_tor_switch = switch - num_tor_switches + 1
                base_vlans = [0, 1] #leftmost agg switch only connected to first two core switches
                if switch % 2 == 0:
                    right_tor_switch = right_tor_switch - 1
                    base_vlans = [2, 3] #rightmost agg switch connected to second two core switches
                right_tor_hosts.extend(get_switch_hosts(right_tor_switch))
                    
                #all hosts from other pods (possible sources)
                outside_pod_switches = get_outside_pod_switches(switch)
                outside_pod_hosts = [] #all hosts in all other pods
                for outside_pod_switch in outside_pod_switches:
                    outside_pod_hosts.extend(get_switch_hosts(outside_pod_switch))
                    
                for dst in right_tor_hosts:
                    for src in outside_pod_hosts:
                        for vlan in base_vlans:
                            right_tor_vlans.append(get_vlan(vlan, src, dst))
                            
                for inport in core_ports: #incoming traffic port
                    for vlan in right_tor_vlans:
                        from_port_to_port(inport, outport, vlan)
                    
                    
                            
            install_case_1()
            install_case_2()
            install_case_3()
            
        def install_core_rules():
            def get_hosts_by_pod_num(pod):
                num_hosts_pod = 4
                last_host = pod * num_hosts_pod
                first_host = last_host - (num_hosts_pod - 1)
                hosts = range(first_host, last_host + 1)
                return hosts
                
                
            ports = [1, 2, 3, 4]
            
            for inport in ports:
                outports = []
                for port in ports:
                    if port != inport:
                        outports.append(port)
                
                base_core_num = 17
                
                inport_pod_hosts = get_hosts_by_pod_num(inport) #possible sources
                
                for outport in outports:
                    
                    #vlans based on for each outport
                    outport_pod_hosts = get_hosts_by_pod_num(outport)
                    
                    possible_vlans = []
                    for src in inport_pod_hosts:
                        vlan = switch - base_core_num
                        for dst in outport_pod_hosts:
                            possible_vlans.append(get_vlan(vlan, src, dst))
                    
                    for vlan in possible_vlans:
                        from_port_to_port(inport, outport, vlan)
                    
                    
        #run installation functions
        print "starting rule installation"
        if dp.id <= 8:
            install_tor_rules()
        if dp.id > 8 and dp.id <= 16:
            install_agg_rules()
        if dp.id > 16:
            install_core_rules()
            
		
