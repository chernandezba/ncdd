#!/bin/sh

chkconfig > /dev/null 2>&1 
if [ $? == 0 ]; then
	chkconfig --add $1
fi
