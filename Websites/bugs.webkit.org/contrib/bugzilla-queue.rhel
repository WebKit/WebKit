#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


# bugzilla-queue This starts, stops, and restarts the Bugzilla jobqueue.pl
#		 daemon, which manages sending queued mail and possibly
#		 other queued tasks in the future.
#
# chkconfig: 345 85 15
# description: Bugzilla queue runner
#
### BEGIN INIT INFO
# Provides: bugzilla-queue
# Required-Start: $local_fs $syslog MTA mysqld
# Required-Stop: $local_fs $syslog MTA mysqld
# Default-Start: 3 5
# Default-Stop: 0 1 2 6
# Short-Description: Start and stop the Bugzilla queue runner.
# Description: The Bugzilla queue runner (jobqueue.pl) sends any mail
#	that Bugzilla has queued to be sent in the background. If you
#	have enabled the use_mailer_queue parameter in Bugzilla, you
#	must run this daemon.
### END INIT INFO

NAME=`basename $0`

#################
# Configuration #
#################

# This should be the path to your Bugzilla
BUGZILLA=/var/www/html/bugzilla
# Who owns the Bugzilla directory and files?
USER=root

# If you want to pass any options to the daemon (like -d for debugging)
# specify it here.
OPTIONS=""

# You can also override the configuration by creating a 
# /etc/sysconfig/bugzilla-queue file so that you don't
# have to edit this script. 
if [ -r /etc/sysconfig/$NAME ]; then
  . /etc/sysconfig/$NAME
fi

##########
# Script #
##########

RETVAL=0
BIN=$BUGZILLA/jobqueue.pl
PIDFILE=/var/run/$NAME.pid

# Source function library.
. /etc/rc.d/init.d/functions

usage ()
{
    echo "Usage: service $NAME {start|stop|status|restart|condrestart}"
    RETVAL=1
}


start ()
{
    if [ -f "$PIDFILE" ]; then
	checkpid `cat $PIDFILE` && return 2
    fi
    echo -n "Starting $NAME: "
    touch $PIDFILE
    chown $USER $PIDFILE
    daemon --user=$USER \
        "$BIN ${OPTIONS} -p '$PIDFILE' -n $NAME start > /dev/null"
    ret=$?
    [ $ret -eq "0" ] && touch /var/lock/subsys/$NAME
    echo
    return $ret
}

stop ()
{
    [ -f /var/lock/subsys/$NAME ] || return 2
    echo -n "Killing $NAME: "
    killproc $NAME
    echo
    rm -f /var/lock/subsys/$NAME
}

restart ()
{
    stop
    start
}

condrestart ()
{
    [ -e /var/lock/subsys/$NAME ] && restart || return 2
}


case "$1" in
    start) start; RETVAL=$? ;;
    stop) stop; RETVAL=$? ;;
    status) $BIN -p $PIDFILE -n $NAME check; RETVAL=$?;;
    restart) restart; RETVAL=$? ;;
    condrestart) condrestart; RETVAL=$? ;;
    *) usage ;;
esac

exit $RETVAL
