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
# Required-Start: $local_fs $syslog
# Required-Stop: $local_fs $syslog
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

BIN=$BUGZILLA/jobqueue.pl
if [ ! -x $BIN ]; then
    echo "$BIN not installed"
    if [ "$1" = "stop" ]; then
        exit 0
    else
        exit 5
    fi
fi

# Source LSB function library.
. /lib/lsb/init-functions

# Reset status of this service.
rc_reset

# Return values for all commands but status:
# 0       - success
# 1       - generic or unspecified error
# 2       - invalid or excess argument(s)
# 3       - unimplemented feature (e.g. "reload")
# 4       - user had insufficient privileges
# 5       - program is not installed
# 6       - program is not configured
# 7       - program is not running
# 8--199  - reserved (8--99 LSB, 100--149 distrib, 150--199 appl)
#
# Note that starting an already running service, stopping
# or restarting a not-running service as well as the restart
# with force-reload (in case signaling is not supported) are
# considered a success.

case "$1" in
    start)
        echo -n "Starting $NAME "
        # Start daemon with startproc(8).  If this fails the return value
        # is set appropriately by startproc.
        start_daemon -u $USER $BIN ${OPTIONS} start

        # Remember status and be verbose
        rc_status -v
        ;;

    stop)
        echo -n "Shutting down $NAME "
        # Stop daemon with killproc(8) and if this fails killproc sets the
        # return value according to LSB.
        killproc -TERM $BIN

        # Remember status and be verbose
        rc_status -v
        ;;

    status)
        echo -n "Checking for service $NAME "
        # Check status with checkproc(8), if process is running checkproc
        # will return with exit status 0.

        # Return value is slightly different for the status command:
        # 0 - service up and running
        # 1 - service dead, but /var/run/  pid  file exists
        # 2 - service dead, but /var/lock/ lock file exists
        # 3 - service not running (unused)
        # 4 - service status unknown :-(
        # 5--199 reserved (5--99 LSB, 100--149 distro, 150--199 appl.)

        # NOTE: checkproc returns LSB compliant status values.
        checkproc $BIN

        # NOTE: rc_status knows that we called this init script with
        # "status" option and adapts its messages accordingly.
        rc_status -v

        # Run jobqueue's own check function too.
        $BIN check
        ;;

    restart)
        # Stop the service and regardless of whether it was running or not,
        # start it again.
        $0 stop
        $0 start

        # Remember status and be quiet.
        rc_status
        ;;

    try-restart|condrestart)
        # Do a restart only if the service was active before.
        # NOTE: try-restart is now part of LSB (as of 1.9). RH has a
        # similar command named condrestart.
        if [ "$1" = "condrestart" ]; then
            echo "${attn} Use try-restart ${done}(LSB)${attn} rather than condrestart ${warn}(RH)${norm}"
        fi
        $0 status
        if [ $? -eq 0 ]; then
            $0 restart
        else
            rc_reset        # Not running is not a failure.
        fi

        # Remember status and be quiet
        rc_status
        ;;

    force-reload)
        # The jobqueue.pl daemon does not support SIGHUP for reload.  Just
        # restart the service if it is running.
        echo -n "Reload service $NAME "

        $0 try-restart
        rc_status
        ;;

    reload)
        # The jobqueue.pl daemon does not support SIGHUP for reload.
        rc_failed 3
        rc_status -v
        ;;

    *)
        echo "Usage: $0 {start|stop|status|try-restart|restart|force-reload|reload}"
        exit 1
esac

rc_exit
