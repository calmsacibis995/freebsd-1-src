#!/bin/sh
#
# whatis -- search the whatis database for keywords.  Like apropos,
#           but match only commands (as whole words).
#
# Copyright (c) 1990, 1991, John W. Eaton.
#
# You may distribute under the terms of the GNU General Public
# License as specified in the README file that comes with the man
# distribution.  
#
# John W. Eaton
# jwe@che.utexas.edu
# Department of Chemical Engineering
# The University of Texas at Austin
# Austin, Texas  78712

PATH=/usr/local/bin:/bin:/usr/ucb:/usr/bin

libdir=%libdir%

if [ $# = 0 ]
then
    echo "usage: `basename $0` name ..."
    exit 1
fi

manpath=`%bindir%/manpath -q | tr : '\040'`

if [ "$manpath" = "" ]
then
    echo "whatis: manpath is null"
    exit 1
fi

if [ "$PAGER" = "" ]
then
    PAGER="%pager%"
fi

while [ $1 ]
do
    found=0
    for d in $manpath /usr/lib
    do
        if [ -f $d/whatis ]
        then
            grep -iw "^$1" $d/whatis
            status=$?
            if [ "$status" = "0" ]
            then
                found=1
                export found;
            fi
        fi
    done

    if [ "$found" = "0" ]
    then
        echo "$1: nothing appropriate"
    fi

    shift
done | $PAGER

exit
