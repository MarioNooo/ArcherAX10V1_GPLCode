#!/bin/sh
# dslite.sh - IPv4-in-IPv6 tunnel backend
# Copyright (c) 2013 OpenWrt.org

[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. /lib/functions/network.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_dslite_setup() {
	local cfg="$1"
	local iface="$2"
	local link="dslt-$cfg"
	local tunlink="wanv6"
	local count=0
	local flag=0
	local addr_temp
	#local STDOUT=/dev/console
	local STDOUT=/dev/null

	local mtu ttl peeraddr ip6addr dns_server dns_pri dns_snd server
	json_get_vars mtu ttl  peeraddr ip6addr

	echo dslite start:$cfg > /dev/console

	[ -z "$ip6addr" ] && { 
		local wanif="$tunlink" 
		if [ -z "$wanif" ] && ! network_find_wan6 wanif; then 
			proto_notify_error "$cfg" "NO_WAN_LINK" 
			sleep 5 
			proto_setup_failed "$cfg" 
			return 
		fi 
        
		if ! network_get_ipaddr6 ip6addr "$wanif"; then 
			proto_notify_error "$cfg" "NO_WAN_LINK" 
			sleep 5 
			proto_setup_failed "$cfg" 
			return 
		fi 
		network_get_dnsserver dns_server "$wanif"
	}

	echo "ip6addr : $ip6addr" > $STDOUT
	config_load /etc/config/network
	config_get server $cfg AFTR_name

	echo "server: $server" > $STDOUT
	echo "dns server : $dns_server" > $STDOUT
	[ -n "$server" ] && [ -z "$peeraddr" ] && {
		dns_pri=`echo $dns_server|awk -F ' ' '{print $1}'`
		dns_snd=`echo $dns_server|awk -F ' ' '{print $2}'`
		echo dns_pri $dns_pri dns_snd $dns_snd > $STDOUT
		peeraddr=`dnslookup -6 -t 1 $server $dns_pri`
		echo "pridnslookup -6 -t 1 $server $dns_pri" > $STDOUT

		[ -z "$peeraddr" ] && {
			peeraddr=`dnslookup -6 -t 1 $server $dns_snd`
			echo "snddnslookup -6 -t 1 $server $dns_snd" > $STDOUT
		}

		[ -n "$peeraddr" ] && {
			for addr in $peeraddr; do
				local ping_success=`ping -6 $addr -c 1 -W 3|grep "bytes from"`
				count=`expr $count + 1`
				[ $count == 1 ] && addr_temp=$addr
				[ -n "$ping_success" ] && {
					flag=1
					echo "ping success,peeraddr=$addr" > $STDOUT
					peeraddr=$addr
					break
				}
			done
			[ $flag == 0 ] && peeraddr=$addr_temp
			echo $peeraddr > /tmp/dslt-dnsreplyUsed
		}

	}

	[ -z "$peeraddr" ] && {
		echo "peeraddr is null" > $STDOUT
		proto_notify_error "$cfg" "MISSING_ADDRESS"
		#proto_block_restart "$cfg"
		sleep 10
		proto_setup_failed "$cfg"
		return
	}
		
	echo "peer2: $peeraddr" > $STDOUT

	# stop brcm pktrunner
	[ -f /lib/modules/*/extra/pktrunner.ko ] && rmmod pktrunner

	mtu=${mtu:-1280}
	tunlink=${tunlink:-wanv6}
	/lib/netifd/dslite-up.sh $link $mtu $ip6addr $peeraddr $tunlink $cfg
	
	if [ -n "$server" ]; then
		proto_run_command "$cfg" /usr/sbin/dslite \
			-i "$link" \
			-m "$mtu" \
			-l "$ip6addr" \
			-6 "$tunlink" \
			-s "$server" \
			"$cfg"
	fi
}

proto_dslite_teardown() {
	local cfg="$1"
	local link="dslt-$cfg"
	echo dslite teardown > /dev/console
	
	proto_kill_command $cfg
	# insmod brcm pktrunner
	[ -f /lib/modules/*/extra/pktrunner.ko ] && insmod pktrunner
}

proto_dslite_init_config() {            
	available=1
	proto_config_add_string "AFTR_name"
	proto_config_add_string "ip6addr"
	proto_config_add_string "peeraddr"
	proto_config_add_string "tunlink"
	proto_config_add_int "mtu"
	proto_config_add_int "ttl"
}

[ -n "$INCLUDE_ONLY" ] || {
	add_protocol dslite
}
