#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# mysqld-watcher.pl - a script that watches the running instance of
# mysqld and kills off any long-running SELECTs against the shadow_db
# 
use 5.10.1;
use strict;
use warnings;

# some configurables: 

# length of time before a thread is eligible to be killed, in seconds
#
my $long_query_time = 180;
#
# the From header for any messages sent out
#
my $mail_from = "root\@mothra.mozilla.org";
#
# the To header for any messages sent out
#
my $mail_to = "root";
#
# mail transfer agent.  this should probably really be converted to a Param().
#
my $mta_program = "/usr/lib/sendmail -t -ODeliveryMode=deferred";

# The array of long-running queries
#
my $long = {};

# Run mysqladmin processlist twice, the first time getting complete queries
# and the second time getting just abbreviated queries.  We want complete
# queries so we know which queries are taking too long to run, but complete
# queries with line breaks get missed by this script, so we get abbreviated
# queries as well to make sure we don't miss any.
foreach my $command ("/opt/mysql/bin/mysqladmin --verbose processlist", 
                     "/opt/mysql/bin/mysqladmin processlist")
{
    close(STDIN);
    open(STDIN, "$command |");

    # iterate through the running threads
    #
    while ( <STDIN> ) { 
        my @F = split(/\|/);

        # if this line is not the correct number of fields, or if the thread-id
        # field contains Id, skip this line.  both these cases indicate that this
        # line contains pretty-printing gunk and not thread info.
        #
        next if ( $#F != 9 || $F[1] =~ /Id/); 

        if ( $F[4] =~ /shadow_bugs/             # shadowbugs database in use
             && $F[5] =~ /Query/                # this is actually a query
             && $F[6] > $long_query_time        # this query has taken too long
             && $F[8] =~ /(select|SELECT)/      # only kill a select
             && !defined($long->{$F[1]}) )      # haven't seen this one already
        {
            $long->{$F[1]} = \@F;
            system("/opt/mysql/bin/mysqladmin", "kill", $F[1]);
        }
    }
}

# send an email message
#
# should perhaps be moved to somewhere more global for use in bugzilla as a 
# whole; should also do more error-checking
#
sub sendEmail($$$$) {
    ($#_ == 3) || die("sendEmail: invalid number of arguments");
    my ($from, $to, $subject, $body) = @_;

    open(MTA, "|$mta_program");
    print MTA "From: $from\n";
    print MTA "To: $to\n";
    print MTA "Subject: $subject\n";
    print MTA "\n";
    print MTA $body;
    print MTA "\n";
    close(MTA);
   
}

# if we found anything, kill the database thread and send mail about it
#
if (scalar(keys(%$long))) {
    my $message = "";
    foreach my $process_id (keys(%$long)) {
        my $qry = $long->{$process_id};
        $message .= join(" ", @$qry) . "\n\n";
    }

    # fire off an email telling the maintainer that we had to kill some threads
    #
    sendEmail($mail_from, $mail_to, "long running MySQL thread(s) killed", $message);
}
