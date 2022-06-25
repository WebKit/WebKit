# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::JobQueue;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Install::Util qw(install_string);
use Bugzilla::Util qw(read_text);
use File::Basename;
use base qw(TheSchwartz);
use fields qw(_worker_pidfile);

# This maps job names for Bugzilla::JobQueue to the appropriate modules.
# If you add new types of jobs, you should add a mapping here.
use constant JOB_MAP => {
    send_mail => 'Bugzilla::Job::Mailer',
    bug_mail  => 'Bugzilla::Job::BugMail',
};

# Without a driver cache TheSchwartz opens a new database connection
# for each email it sends.  This cached connection doesn't persist
# across requests.
use constant DRIVER_CACHE_TIME => 300; # 5 minutes

# To avoid memory leak/fragmentation, a worker process won't process more than
# MAX_MESSAGES messages.
use constant MAX_MESSAGES => 1000;

sub job_map {
    if (!defined(Bugzilla->request_cache->{job_map})) {
        my $job_map = JOB_MAP;
        Bugzilla::Hook::process('job_map', { job_map => $job_map });
        Bugzilla->request_cache->{job_map} = $job_map;
    }
    
    return Bugzilla->request_cache->{job_map};
}

sub new {
    my $class = shift;

    if (!Bugzilla->feature('jobqueue')) {
        ThrowUserError('feature_disabled', { feature => 'jobqueue' });
    }

    my $lc = Bugzilla->localconfig;
    # We need to use the main DB as TheSchwartz module is going
    # to write to it.
    my $self = $class->SUPER::new(
        databases => [{
            dsn    => Bugzilla->dbh_main->{private_bz_dsn},
            user   => $lc->{db_user},
            pass   => $lc->{db_pass},
            prefix => 'ts_',
        }],
        driver_cache_expiration => DRIVER_CACHE_TIME,
        prioritize => 1,
    );

    return $self;
}

# A way to get access to the underlying databases directly.
sub bz_databases {
    my $self = shift;
    my @hashes = keys %{ $self->{databases} };
    return map { $self->driver_for($_) } @hashes;
}

# inserts a job into the queue to be processed and returns immediately
sub insert {
    my $self = shift;
    my $job = shift;

    if (!ref($job)) {
        my $mapped_job = Bugzilla::JobQueue->job_map()->{$job};
        ThrowCodeError('jobqueue_no_job_mapping', { job => $job })
            if !$mapped_job;

        $job = new TheSchwartz::Job(
            funcname => $mapped_job,
            arg      => $_[0],
            priority => $_[1] || 5
        );
    }
    
    my $retval = $self->SUPER::insert($job);
    # XXX Need to get an error message here if insert fails, but
    # I don't see any way to do that in TheSchwartz.
    ThrowCodeError('jobqueue_insert_failed', { job => $job, errmsg => $@ })
        if !$retval;
 
    return $retval;
}

# To avoid memory leaks/fragmentation which tends to happen for long running
# perl processes; check for jobs, and spawn a new process to empty the queue.
sub subprocess_worker {
    my $self = shift;

    my $command = "$0 -d -p '" . $self->{_worker_pidfile} . "' onepass";

    while (1) {
        my $time = (time);
        my @jobs = $self->list_jobs({
            funcname      => $self->{all_abilities},
            run_after     => $time,
            grabbed_until => $time,
            limit         => 1,
        });
        if (@jobs) {
            $self->debug("Spawning queue worker process");
            # Run the worker as a daemon
            system $command;
            # And poll the PID to detect when the working has finished.
            # We do this instead of system() to allow for the INT signal to
            # interrup us and trigger kill_worker().
            my $pid = read_text($self->{_worker_pidfile}, err_mode => 'quiet');
            if ($pid) {
                sleep(3) while(kill(0, $pid));
            }
            $self->debug("Queue worker process completed");
        } else {
            $self->debug("No jobs found");
        }
        sleep(5);
    }
}

sub kill_worker {
    my $self = Bugzilla->job_queue();
    if ($self->{_worker_pidfile} && -e $self->{_worker_pidfile}) {
        my $worker_pid = read_text($self->{_worker_pidfile});
        if ($worker_pid && kill(0, $worker_pid)) {
            $self->debug("Stopping worker process");
            system "$0 -f -p '" . $self->{_worker_pidfile} . "' stop";
        }
    }
}

sub set_pidfile {
    my ($self, $pidfile) = @_;
    $self->{_worker_pidfile} = bz_locations->{'datadir'} .
                               '/worker-' . basename($pidfile);
}

# Clear the request cache at the start of each run.
sub work_once {
    my $self = shift;
    Bugzilla->clear_request_cache();
    return $self->SUPER::work_once(@_);
}

# Never process more than MAX_MESSAGES in one batch, to avoid memory
# leak/fragmentation issues.
sub work_until_done {
    my $self = shift;
    my $count = 0;
    while ($count++ < MAX_MESSAGES) {
        $self->work_once or last;
    }
}

1;

__END__

=head1 NAME

Bugzilla::JobQueue - Interface between Bugzilla and TheSchwartz.

=head1 SYNOPSIS

 use Bugzilla;

 my $obj = Bugzilla->job_queue();
 $obj->insert('send_mail', { msg => $message });

=head1 DESCRIPTION

Certain tasks should be done asyncronously.  The job queue system allows
Bugzilla to use some sort of service to schedule jobs to happen asyncronously.

=head2 Inserting a Job

See the synopsis above for an easy to follow example on how to insert a
job into the queue.  Give it a name and some arguments and the job will
be sent away to be done later.

=head1 B<Methods in need of POD>

=over

=item insert

=item bz_databases

=item job_map

=item set_pidfile

=item kill_worker

=back
