#!/bin/sh
case "$1" in
stop) killall radiusd;;
*) [ -x /usr/local/sbin/radiusd ] && /usr/local/sbin/radiusd -y && echo -n ' radiusd';;
esac
