#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use Cwd qw(abs_path);
use File::Basename;
BEGIN {
    # Untaint the abs_path.
    my ($a) = abs_path($0) =~ /^(.*)$/;
    chdir dirname($a);
}

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
             any, up to a limit of 1000 items) and then exits
   onepass   Checks the job queue, executes all items found, and then exits
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
