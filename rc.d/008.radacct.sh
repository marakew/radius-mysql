#!/bin/sh

radclient="/usr/local/bin/radclient"
server=127.0.0.1
secrets=testing123

#usage () {
#        echo "Usage: $0 radius-server secret" >&2
#        exit 1
#}

#if [ $# -lt 2 ]
#then
#        usage
#fi

case "$1" in
        start)
                echo -n ' NAS Accounting-On: '
		(
		        echo "Acct-Status-Type = Accounting-On" 
		) | ${radclient} ${server} acct ${secrets}
                ;;
        stop)
		echo -n ' NAS Accounting-Off: '
		(
		        echo "Acct-Status-Type = Accounting-Off" 
		) | ${radclient} ${server} acct ${secrets}
		;;
        -h)
                echo "Usage: `basename $0` { start | stop }"
                ;;
        *)
                ;;

esac

