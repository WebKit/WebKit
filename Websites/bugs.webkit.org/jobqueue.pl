#!/usr/bin/env perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# Mozilla Corporation. All Rights Reserved.
#
# Contributor(s): 
#   Mark Smith <mark@mozilla.com>
#   Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use File::Basename;
BEGIN { chdir dirname($0); }

use lib qw(. lib);
use Bugzilla;
use Bugzilla::JobQueue::Runner;

Bugzilla::JobQueue::Runner->new();

=head1 NAME

jobqueue.pl - Runs jobs in the background for Bugzilla.

=head1 SYNOPSIS

 ./jobqueue.pl [OPTIONS] COMMAND

   OPTIONS:
   -f        Run in the foreground (don't detach)
   -d        Output a lot of debugging information
   -p file   Specify the file where jobqueue.pl should store its current
             process id. Defaults to F<data/jobqueue.pl.pid>.
   -n name   What should this process call itself in the system log?
             Defaults to the full path you used to invoke the script.

   COMMANDS:
   start     Starts a new jobqueue daemon if there isn't one running already
   stop      Stops a running jobqueue daemon
   restart   Stops a running jobqueue if one is running, and then
             starts a new one.
   once      Checks the job queue once, executes the first item found (if
             any) and then exits
   check     Report the current status of the daemon.
   install   On some *nix systems, this automatically installs and
             configures jobqueue.pl as a system service so that it will
             start every time the machine boots.
   uninstall Removes the system service for jobqueue.pl.
   help      Display this usage info
   version   Display the version of jobqueue.pl

=head1 DESCRIPTION

See L<Bugzilla::JobQueue> and L<Bugzilla::JobQueue::Runner>.

=head1 Running jobqueue.pl as a System Service

For systems that use Upstart or SysV Init, there is a SysV/Upstart init
script included with Bugzilla for jobqueue.pl: F<contrib/bugzilla-queue>.
It should work out-of-the-box on RHEL, Fedora, CentOS etc.

You can install it by doing C<./jobqueue.pl install> as root, after
already having run L<checksetup> at least once to completion
on this Bugzilla installation.

If you are using a system that isn't RHEL, Fedora, CentOS, etc., then you
may have to modify F<contrib/bugzilla-queue> and install it yourself
manually in order to get C<jobqueue.pl> running as a system service.
