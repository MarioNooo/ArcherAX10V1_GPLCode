#!/bin/sh
. /usr/lib/modem/usbmodem_log.sh
[ -n "$INCLUDE_ONLY" ] || {
	NOT_INCLUDED=1
	INCLUDE_ONLY=1

	. ../netifd-proto.sh
	. ./ppp.sh
	init_proto "$@"
}

proto_3g_init_config() {
	no_device=1
	available=1
	ppp_generic_init_config
	proto_config_add_string "device"
	proto_config_add_string "apn"
	proto_config_add_string "auth"
	proto_config_add_string "username"
	proto_config_add_string "password"
	proto_config_add_string "dialnumber"
	syslog $LOG_PROTO_INIT "proto=3g"
}

start_3g_dial(){
    device=$1
	apn=$2
	auth=$3 
	username=$4 
	password=$5 
	dialnumber=$6
	
	local pinlock=$(uci get modem.modemisp.pinlock)
	syslog $LOG_PROTO_SETUP "proto=3g device=$device apn=$apn auth=$auth username=$username dialnumber=$dialnumber pinlock=$pinlock"
	
    [ -e "$device" ] || {
        proto_set_available "$interface" 0
        return 1
    }

    #[ -n "$apn" ] || {
    #    proto_set_available "$interface" 0
    #   return 1
    #}

    local pinlock=$(uci get modem.modemisp.pinlock)
    if [ $pinlock -ne 0 -a $pinlock -ne 4 ]; then
        proto_notify_error "$interface" PIN_FAILED
        proto_block_restart "$interface"
        return 1
    fi

    if [ -z "$dialnumber" ]; then
        dialnumber="*99***1#"
    fi

	local sig_res=""
	sig_res=$(gcom -d ${device} -s /etc/gcom/getstrength.gcom 2>/dev/null)
	#echo "sig_res:$sig_res" >>/dev/console
	[ -z $sig_res] || {
		rssi=$(echo "$sig_res" | awk '{
		if($0 ~ /CSQ:/ )
		{
			split($0,arr_tmp," ")
			split(arr_tmp[2],arr,",")
			print(arr[1])
		}
		}')
		#echo "rssi:$rssi" >>/dev/console
		sig_flag=$(expr match $rssi "[0-9]*$")
		if [ $sig_flag -gt 0 ];then
			uci set modem.modemisp.modem_signal=$rssi
			uci commit modem
		fi
		#st=$(uci get modem.modemconf.modem_signal)
		#echo "modemconf st:$st*************************************" >>/dev/console
	}		
	syslog $LOG_GET_SIGNAL "gcom -d ${device} -s /etc/gcom/getstrength.gcom" "$sig_res" "$rssi"

    export SETUSER=$username
    export SETPASS=$password
    export SETAUTH=$auth             
    
    connect="${apn:+USE_APN=$apn }DIALNUMBER=$dialnumber /usr/sbin/chat -t5 -v -E -f $chat"
    ppp_generic_setup "$interface" \
        noaccomp \
        nopcomp \
        novj \
        nobsdcomp \
        noauth \
        lock \
        crtscts \
        115200 "$device"

}

proto_3g_setup() {
	local interface="$1"
	local chat="/tmp/modem/3g.chat"
    local ttylist="/tmp/modem/ttylist"
	local device apn auth username password dialnumber
	json_get_vars device apn auth username password dialnumber

    [ -e $chat ] && {
        rm -rf $chat
    }

if [ -n "$apn" ] && [ "$apn" != "0" ];then
echo "ABORT   BUSY
ABORT   'NO CARRIER'
ABORT   ERROR
REPORT  CONNECT
ABORT   'NO ANSWER'
TIMEOUT 10
\"\"      \"AT\"
OK      \"ATZ\"
OK      'AT+CGDCONT=1,\"IP\",\"$apn\"'
SAY     \"Calling UMTS/GPRS\"
TIMEOUT 30
OK      \"ATDT$dialnumber\"
CONNECT ' '" >$chat
else
echo "ABORT   BUSY
ABORT   'NO CARRIER'
ABORT   ERROR
ABORT   'NO DIAL TONE'
ABORT   'NO ANSWER'
ABORT   DELAYED
REPORT  CONNECT
TIMEOUT 10
''              AT
OK              ATZ
SAY     'Calling CDMA/EVDO'
TIMEOUT 30
OK              ATDT$dialnumber
CONNECT ''" >$chat
fi
	
    testTTY=$(uci get network.mobile.testTTY)
    if [ $testTTY = "1" ];then
	    syslog $LOG_PROTO_SETUP "testTTY=1,start test dial each tty."
        dialed=0
        [ -e $ttylist ] && {
            while read line
            do
                if [ -z $line ];then
                    return 1
                fi
                device="/dev/$line"
                nextTryTTY=$(uci get network.mobile.nextTryTTY)
				#syslog $LOG_PROTO_SETUP "test tty,device=$device nextTryTTY=$nextTryTTY."
                if [ -z $nextTryTTY ];then
                    uci set network.mobile.device=$device
                    uci set network.mobile.nextTryTTY=$device
                    uci commit
                    /etc/init.d/network reload
					syslog $LOG_PROTO_SETUP "test tty,device=$device nextTryTTY=$nextTryTTY,run network reload."
                    return 1
                else
                    if [ $nextTryTTY=$device ];then
                        wait_modem_register=0
						syslog $LOG_PROTO_SETUP "wait modem register,device=$device nextTryTTY=$nextTryTTY."
                        while [ $wait_modem_register -le 40 ];do
                            state=$(gcom -d $device -s /etc/gcom/getregistestate.gcom | sed -n '/^+CREG:/'p | awk -F ',' '{print $2}' | awk '{print substr($0,1,1)}')
                            if [ $state != 1 ];then
                                sleep 1
                                wait_modem_register=$(($wait_modem_register+1))                                
                            else
                                break
                            fi                        
                        done
                        
                        if [ $wait_modem_register -ge 40 ];then
							syslog $LOG_PROTO_SETUP "modem register fail."
                            echo "**registered faild">/dev/console
                            return 1
                        fi
						syslog $LOG_PROTO_SETUP "modem register success.run start_3g_dial..."
                        start_3g_dial $device $apn $auth $username $password $dialnumber
                        sec_index=0
                        while [ $sec_index -le 20 ];do
                            result=$(ubus call network.interface.mobile status)
                            if echo $result | grep -q '\"connecting\"'
                            then
                                sleep 2
                                sec_index=$(($sec_index+2))
                            else
                                if echo $result | grep -q '\"connected\"'      
                                then
									syslog $LOG_PROTO_SETUP "3g connected."
                                    uci set network.mobile.testTTY="0"
                                    uci commit
                                    return 0
                                else
                                    return 0 
                                fi                            
                            fi
                        done
     
                        dialed=1
                    else
                        if [ $dialed=1 ];then
                            uci set network.mobile.device=$device
                            uci set network.mobile.nextTryTTY=$device
                            uci commit
                            /etc/init.d/network reload
							syslog $LOG_PROTO_SETUP "test tty,device=$device nextTryTTY=$nextTryTTY,run network reload."
                            return 1
                        fi
                        
                    fi
                fi
            done < $ttylist 
            uci set network.mobile.device=""
            uci set network.mobile.nextTryTTY=""
            uci commit
            syslog $LOG_PROTO_SETUP "testTTY end，not found available tty interface."
        }
    else 
		local pinlock=$(uci get modem.modemisp.pinlock)
		syslog $LOG_PROTO_SETUP "proto=3g device=$device apn=$apn auth=$auth username=$username dialnumber=$dialnumber pinlock=$pinlock"
		
        [ -e "$device" ] || {
            proto_set_available "$interface" 0
            return 1
        }
        #[ -n "$apn" ] || {
        #    proto_set_available "$interface" 0
        #    return 1
        #}
        
        if [ $pinlock -ne 0 -a $pinlock -ne 4 ]; then
            proto_notify_error "$interface" PIN_FAILED
            proto_block_restart "$interface"
            return 1
        fi
        
        if [ -z "$dialnumber" ]; then
            dialnumber="*99***1#"
        fi

		local sig_res=""
		sig_res=$(gcom -d ${device} -s /etc/gcom/getstrength.gcom 2>/dev/null)
		#echo "sig_res:$sig_res" >>/dev/console
		[ -z $sig_res] || {
			rssi=$(echo "$sig_res" | awk '{
			if($0 ~ /CSQ:/ )
			{
				split($0,arr_tmp," ")
				split(arr_tmp[2],arr,",")
				print(arr[1])
			}
			}')
			#echo "rssi:$rssi" >>/dev/console
			sig_flag=$(expr match $rssi "[0-9]*$")
			if [ $sig_flag -gt 0 ];then
				uci set modem.modemisp.modem_signal=$rssi
				uci commit modem
			fi			
			#st=$(uci get modem.modemconf.modem_signal)
			#echo "modemconf st:$st*************************************" >>/dev/console
		}		
		syslog $LOG_GET_SIGNAL "gcom -d ${device} -s /etc/gcom/getstrength.gcom" "$sig_res" "$rssi"
		
        export SETUSER=$username
        export SETPASS=$password
        export SETAUTH=$auth
        #OX=$(gcom -d $device -s /etc/gcom/connect-ppp.gcom 2>&1)
        #if echo $OX | grep -q 'ERROR' 
        #then
           # log "Connection Failed for Modem $CURRMODEM on Authorization"
           # proto_set_available "$interface" 0
           # return 1
        #fi
        connect="${apn:+USE_APN=$apn }DIALNUMBER=$dialnumber /usr/sbin/chat -t5 -v -E -f $chat"
        ppp_generic_setup "$interface" \
            noaccomp \
            nopcomp \
            novj \
            nobsdcomp \
            noauth \
            lock \
            crtscts \
            115200 "$device"
        return 0    
    fi
    return 0
}

proto_3g_teardown() {
	proto_kill_command "$interface"
	ppp_generic_teardown "$@"
	syslog $LOG_PROTO_TEARDOWN "proto=3g $@"
}

[ -z "$NOT_INCLUDED" ] || add_protocol 3g
