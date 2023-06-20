#!/bin/sh

if [ ! -e $1 ]; then
mknod $1 $2 $3 $4
fi
