#!/bin/sh

D=`date -R`
echo \#define NCDD_DATE \"${D}\" > ncdd_date.h

