package Parallel::ForkManager;
our $AUTHORITY = 'cpan:DLUX';
# ABSTRACT:  A simple parallel processing fork manager
$Parallel::ForkManager::VERSION = '1.19';
use POSIX ":sys_wait_h";
use Storable qw(store retrieve);
use File::Spec;
use File::Temp ();
use File::Path ();
use Carp;

use strict;

sub new {
  my ($c,$processes,$tempdir)=@_;

  my $h={
    max_proc   => $processes,
    processes  => {},
    in_child   => 0,
    parent_pid => $$,
    auto_cleanup => ($tempdir ? 0 : 1),
    waitpid_blocking_sleep => 1,
  };


  # determine temporary directory for storing data structures
  # add it to Parallel::ForkManager object so children can use it
  # We don't let it clean up so it won't do it in the child process
  # but we have our own DESTROY to do that.
  if (not defined($tempdir) or not length($tempdir)) {
    $tempdir = File::Temp::tempdir(CLEANUP => 0);
  }
  die qq|Temporary directory "$tempdir" doesn't exist or is not a directory.| unless (-e $tempdir && -d _);  # ensure temp dir exists and is indeed a directory
  $h->{tempdir} = $tempdir;

  return bless($h,ref($c)||$c);
};

sub start {
  my ($s,$identification)=@_;

  die "Cannot start another process while you are in the child process"
    if $s->{in_child};
  while ($s->{max_proc} && ( keys %{ $s->{processes} } ) >= $s->{max_proc}) {
    $s->on_wait;
    $s->wait_one_child(defined $s->{on_wait_period} ? &WNOHANG : undef);
  };
  $s->wait_children;
  if ($s->{max_proc}) {
    my $pid=fork();
    die "Cannot fork: $!" if !defined $pid;
    if ($pid) {
      $s->{processes}->{$pid}=$identification;
      $s->on_start($pid,$identification);
    } else {
      $s->{in_child}=1 if !$pid;
    }
    return $pid;
  } else {
    $s->{processes}->{$$}=$identification;
    $s->on_start($$,$identification);
    return 0; # Simulating the child which returns 0
  }
}

sub start_child {
    my $self = shift;
    my $sub = pop;
    my $identification = shift;

    $self->start( $identification ) # in the parent
            # ... or the child
        or $self->finish( 0, $sub->() );
}


sub finish {
  my ($s, $x, $r)=@_;

  if ( $s->{in_child} ) {
    if (defined($r)) {  # store the child's data structure
      my $storable_tempfile = File::Spec->catfile($s->{tempdir}, 'Parallel-ForkManager-' . $s->{parent_pid} . '-' . $$ . '.txt');
      my $stored = eval { return &store($r, $storable_tempfile); };

      # handle Storables errors, IE logcarp or carp returning undef, or die (via logcroak or croak)
      if (not $stored or $@) {
        warn(qq|The storable module was unable to store the child's data structure to the temp file "$storable_tempfile":  | . join(', ', $@));
      }
    }
    CORE::exit($x || 0);
  }
  if ($s->{max_proc} == 0) { # max_proc == 0
    $s->on_finish($$, $x ,$s->{processes}->{$$}, 0, 0, $r);
    delete $s->{processes}->{$$};
  }
  return 0;
}

sub wait_children {
  my ($s)=@_;

  return if !keys %{$s->{processes}};
  my $kid;
  do {
    $kid = $s->wait_one_child(&WNOHANG);
  } while defined $kid and ( $kid > 0 or $kid < -1 ); # AS 5.6/Win32 returns negative PIDs
};

*wait_childs=*wait_children; # compatibility
*reap_finished_children=*wait_children; # behavioral synonym for clarity

sub wait_one_child {
  my ($s,$par)=@_;

  my $kid;
  while (1) {
    $kid = $s->_waitpid(-1,$par||=0);

    last unless defined $kid;

    last if $kid == 0 || $kid == -1; # AS 5.6/Win32 returns negative PIDs
    redo if !exists $s->{processes}->{$kid};
    my $id = delete $s->{processes}->{$kid};

    # retrieve child data structure, if any
    my $retrieved = undef;
    my $storable_tempfile = File::Spec->catfile($s->{tempdir}, 'Parallel-ForkManager-' . $s->{parent_pid} . '-' . $kid . '.txt');
    if (-e $storable_tempfile) {  # child has option of not storing anything, so we need to see if it did or not
      $retrieved = eval { return &retrieve($storable_tempfile); };

      # handle Storables errors
      if (not $retrieved or $@) {
        warn(qq|The storable module was unable to retrieve the child's data structure from the temporary file "$storable_tempfile":  | . join(', ', $@));
      }

      # clean up after ourselves
      unlink $storable_tempfile;
    }

    $s->on_finish( $kid, $? >> 8 , $id, $? & 0x7f, $? & 0x80 ? 1 : 0, $retrieved);
    last;
  }
  $kid;
};

sub wait_all_children {
  my ($s)=@_;

  while (keys %{ $s->{processes} }) {
    $s->on_wait;
    $s->wait_one_child(defined $s->{on_wait_period} ? &WNOHANG : undef);
  };
}

*wait_all_childs=*wait_all_children; # compatibility;

sub max_procs { $_[0]->{max_proc}; }

sub is_child  { $_[0]->{in_child} }

sub is_parent { !$_[0]->{in_child} }

sub running_procs {
    my $self = shift;

    my @pids = keys %{ $self->{processes} };
    return @pids;
}

sub wait_for_available_procs {
    my( $self, $nbr ) = @_;
    $nbr ||= 1;

    croak "nbr processes '$nbr' higher than the max nbr of processes (@{[ $self->max_procs ]})"
        if $nbr > $self->max_procs;

    $self->wait_one_child until $self->max_procs - $self->running_procs >= $nbr;
}

sub run_on_finish {
  my ($s,$code,$pid)=@_;

  $s->{on_finish}->{$pid || 0}=$code;
}

sub on_finish {
  my ($s,$pid,@par)=@_;

  my $code=$s->{on_finish}->{$pid} || $s->{on_finish}->{0} or return 0;
  $code->($pid,@par);
};

sub run_on_wait {
  my ($s,$code, $period)=@_;

  $s->{on_wait}=$code;
  $s->{on_wait_period} = $period;
}

sub on_wait {
  my ($s)=@_;

  if(ref($s->{on_wait}) eq 'CODE') {
    $s->{on_wait}->();
    if (defined $s->{on_wait_period}) {
        local $SIG{CHLD} = sub { } if ! defined $SIG{CHLD};
        select undef, undef, undef, $s->{on_wait_period}
    };
  };
};

sub run_on_start {
  my ($s,$code)=@_;

  $s->{on_start}=$code;
}

sub on_start {
  my ($s,@par)=@_;

  $s->{on_start}->(@par) if ref($s->{on_start}) eq 'CODE';
};

sub set_max_procs {
  my ($s, $mp)=@_;

  $s->{max_proc} = $mp;
}

sub set_waitpid_blocking_sleep {
    my( $self, $period ) = @_;
    $self->{waitpid_blocking_sleep} = $period;
}

sub waitpid_blocking_sleep {
    $_[0]->{waitpid_blocking_sleep};
}

sub _waitpid { # Call waitpid() in the standard Unix fashion.
    my( $self, undef, $flag ) = @_;

    return $flag ? $self->_waitpid_non_blocking : $self->_waitpid_blocking;
}

sub _waitpid_non_blocking {
    my $self = shift;

    for my $pid ( $self->running_procs ) {
        my $p = waitpid $pid, &WNOHANG or next;

        return $pid if $p != -1;

        warn "child process '$pid' disappeared. A call to `waitpid` outside of Parallel::ForkManager might have reaped it.\n";
        # it's gone. let's clean the process entry
        delete $self->{processes}{$pid};
    }

    return;
}

sub _waitpid_blocking {
    my $self = shift;

    # pseudo-blocking
    if( my $sleep_period = $self->{waitpid_blocking_sleep} ) {
        while() {
            my $pid = $self->_waitpid_non_blocking;

            return $pid if defined $pid;

            return unless $self->running_procs;

            select undef, undef, undef, $sleep_period;
        }
    }

    return waitpid -1, 0;
}

sub DESTROY {
  my ($self) = @_;

  if ($self->{auto_cleanup} && $self->{parent_pid} == $$ && -d $self->{tempdir}) {
    File::Path::remove_tree($self->{tempdir});
  }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Parallel::ForkManager - A simple parallel processing fork manager

=head1 VERSION

version 1.19

=head1 SYNOPSIS

  use Parallel::ForkManager;

  my $pm = Parallel::ForkManager->new($MAX_PROCESSES);

  DATA_LOOP:
  foreach my $data (@all_data) {
    # Forks and returns the pid for the child:
    my $pid = $pm->start and next DATA_LOOP;

    ... do some work with $data in the child process ...

    $pm->finish; # Terminates the child process
  }

=head1 DESCRIPTION

This module is intended for use in operations that can be done in parallel
where the number of processes to be forked off should be limited. Typical
use is a downloader which will be retrieving hundreds/thousands of files.

The code for a downloader would look something like this:

  use LWP::Simple;
  use Parallel::ForkManager;

  ...

  my @links=(
    ["http://www.foo.bar/rulez.data","rulez_data.txt"],
    ["http://new.host/more_data.doc","more_data.doc"],
    ...
  );

  ...

  # Max 30 processes for parallel download
  my $pm = Parallel::ForkManager->new(30);

  LINKS:
  foreach my $linkarray (@links) {
    $pm->start and next LINKS; # do the fork

    my ($link, $fn) = @$linkarray;
    warn "Cannot get $fn from $link"
      if getstore($link, $fn) != RC_OK;

    $pm->finish; # do the exit in the child process
  }
  $pm->wait_all_children;

First you need to instantiate the ForkManager with the "new" constructor.
You must specify the maximum number of processes to be created. If you
specify 0, then NO fork will be done; this is good for debugging purposes.

Next, use $pm->start to do the fork. $pm returns 0 for the child process,
and child pid for the parent process (see also L<perlfunc(1p)/fork()>).
The "and next" skips the internal loop in the parent process. NOTE:
$pm->start dies if the fork fails.

$pm->finish terminates the child process (assuming a fork was done in the
"start").

NOTE: You cannot use $pm->start if you are already in the child process.
If you want to manage another set of subprocesses in the child process,
you must instantiate another Parallel::ForkManager object!

=head1 METHODS

The comment letter indicates where the method should be run. P for parent,
C for child.

=over 5

=item new $processes

Instantiate a new Parallel::ForkManager object. You must specify the maximum
number of children to fork off. If you specify 0 (zero), then no children
will be forked. This is intended for debugging purposes.

The optional second parameter, $tempdir, is only used if you want the
children to send back a reference to some data (see RETRIEVING DATASTRUCTURES
below). If not provided, it is set via a call to L<File::Temp>::tempdir().

The new method will die if the temporary directory does not exist or it is not
a directory.

=item start [ $process_identifier ]

This method does the fork. It returns the pid of the child process for
the parent, and 0 for the child process. If the $processes parameter
for the constructor is 0 then, assuming you're in the child process,
$pm->start simply returns 0.

An optional $process_identifier can be provided to this method... It is used by
the "run_on_finish" callback (see CALLBACKS) for identifying the finished
process.

=item start_child [ $process_identifier, ] \&callback

Like C<start>, but will run the C<&callback> as the child. If the callback returns anything,
it'll be passed as the data to transmit back to the parent process via C<finish()>.

=item finish [ $exit_code [, $data_structure_reference] ]

Closes the child process by exiting and accepts an optional exit code
(default exit code is 0) which can be retrieved in the parent via callback.
If the second optional parameter is provided, the child attempts to send
its contents back to the parent. If you use the program in debug mode
($processes == 0), this method just calls the callback.

If the $data_structure_reference is provided, then it is serialized and
passed to the parent process. See RETRIEVING DATASTRUCTURES for more info.

=item set_max_procs $processes

Allows you to set a new maximum number of children to maintain.

=item wait_all_children

You can call this method to wait for all the processes which have been
forked. This is a blocking wait.

=item reap_finished_children

This is a non-blocking call to reap children and execute callbacks independent
of calls to "start" or "wait_all_children". Use this in scenarios where "start"
is called infrequently but you would like the callbacks executed quickly.

=item is_parent

Returns C<true> if within the parent or C<false> if within the child.

=item is_child

Returns C<true> if within the child or C<false> if within the parent.

=item max_procs 

Returns the maximal number of processes the object will fork.

=item running_procs

Returns the pids of the forked processes currently monitored by the
C<Parallel::ForkManager>. Note that children are still reported as running
until the fork manager harvest them, via the next call to
C<start> or C<wait_all_children>.

    my @pids = $pm->running_procs;

    my $nbr_children =- $pm->running_procs;

=item wait_for_available_procs( $n )

Wait until C<$n> available process slots are available.
If C<$n> is not given, defaults to I<1>.

=item waitpid_blocking_sleep 

Returns the sleep period, in seconds, of the pseudo-blocking calls. The sleep
period can be a fraction of second. 

Returns C<0> if disabled. 

Defaults to 1 second.

See I<BLOCKING CALLS> for more details.

=item set_waitpid_blocking_sleep $seconds

Sets the the sleep period, in seconds, of the pseudo-blocking calls.
Set to C<0> to disable.

See I<BLOCKING CALLS> for more details.

=back

=head1 CALLBACKS

You can define callbacks in the code, which are called on events like starting
a process or upon finish. Declare these before the first call to start().

The callbacks can be defined with the following methods:

=over 4

=item run_on_finish $code [, $pid ]

You can define a subroutine which is called when a child is terminated. It is
called in the parent process.

The parameters of the $code are the following:

  - pid of the process, which is terminated
  - exit code of the program
  - identification of the process (if provided in the "start" method)
  - exit signal (0-127: signal name)
  - core dump (1 if there was core dump at exit)
  - datastructure reference or undef (see RETRIEVING DATASTRUCTURES)

=item run_on_start $code

You can define a subroutine which is called when a child is started. It called
after the successful startup of a child in the parent process.

The parameters of the $code are the following:

  - pid of the process which has been started
  - identification of the process (if provided in the "start" method)

=item run_on_wait $code, [$period]

You can define a subroutine which is called when the child process needs to wait
for the startup. If $period is not defined, then one call is done per
child. If $period is defined, then $code is called periodically and the
module waits for $period seconds between the two calls. Note, $period can be
fractional number also. The exact "$period seconds" is not guaranteed,
signals can shorten and the process scheduler can make it longer (on busy
systems).

The $code called in the "start" and the "wait_all_children" method also.

No parameters are passed to the $code on the call.

=back

=head1 BLOCKING CALLS

When it comes to waiting for child processes to terminate, C<Parallel::ForkManager> is between 
a fork and a hard place (if you excuse the terrible pun). The underlying Perl C<waitpid> function
that the module relies on can block until either one specific or any child process 
terminate, but not for a process part of a given group.

This means that the module can do one of two things when it waits for 
one of its child processes to terminate:

=over

=item Only wait for its own child processes

This is done via a loop using a C<waitpid> non-blocking call and a sleep statement.
The code does something along the lines of

    while(1) {
        if ( any of the P::FM child process terminated ) {
            return its pid
        }

        sleep $sleep_period
    }

This is the default behavior that the module will use.
This is not the most efficient way to wait for child processes, but it's
the safest way to ensure that C<Parallel::ForkManager> won't interfere with 
any other part of the codebase. 

The sleep period is set via the method C<set_waitpid_blocking_sleep>.

=item Block until any process terminate

Alternatively, C<Parallel::ForkManager> can call C<waitpid> such that it will
block until any child process terminate. If the child process was not one of
the monitored subprocesses, the wait will resume. This is more efficient, but mean
that C<P::FM> can captures (and discards) the termination notification that a different
part of the code might be waiting for. 

If this is a race condition
that doesn't apply to your codebase, you can set the 
I<waitpid_blocking_sleep> period to C<0>, which will enable C<waitpid> call blocking.

    my $pm = Parallel::ForkManager->new( 4 );

    $pm->set_waitpid_blocking_sleep(0);  # true blocking calls enabled

    for ( 1..100 ) {
        $pm->start and next;

        ...; # do work

        $pm->finish;
    }

=back

=head1 RETRIEVING DATASTRUCTURES from child processes

The ability for the parent to retrieve data structures is new as of version
0.7.6.

Each child process may optionally send 1 data structure back to the parent.
By data structure, we mean a reference to a string, hash or array. The
contents of the data structure are written out to temporary files on disc
using the L<Storable> modules' store() method. The reference is then
retrieved from within the code you send to the run_on_finish callback.

The data structure can be any scalar perl data structure which makes sense:
string, numeric value or a reference to an array, hash or object.

There are 2 steps involved in retrieving data structures:

1) A reference to the data structure the child wishes to send back to the
parent is provided as the second argument to the finish() call. It is up
to the child to decide whether or not to send anything back to the parent.

2) The data structure reference is retrieved using the callback provided in
the run_on_finish() method.

Keep in mind that data structure retrieval is not the same as returning a
data structure from a method call. That is not what actually occurs. The
data structure referenced in a given child process is serialized and
written out to a file by L<Storable>. The file is subsequently read back
into memory and a new data structure belonging to the parent process is
created. Please consider the performance penalty it can imply, so try to
keep the returned structure small.

=head1 EXAMPLES

=head2 Parallel get

This small example can be used to get URLs in parallel.

  use Parallel::ForkManager;
  use LWP::Simple;

  my $pm = Parallel::ForkManager->new(10);

  LINKS:
  for my $link (@ARGV) {
    $pm->start and next LINKS;
    my ($fn) = $link =~ /^.*\/(.*?)$/;
    if (!$fn) {
      warn "Cannot determine filename from $fn\n";
    } else {
      $0 .= " " . $fn;
      print "Getting $fn from $link\n";
      my $rc = getstore($link, $fn);
      print "$link downloaded. response code: $rc\n";
    };
    $pm->finish;
  };

=head2 Callbacks

Example of a program using callbacks to get child exit codes:

  use strict;
  use Parallel::ForkManager;

  my $max_procs = 5;
  my @names = qw( Fred Jim Lily Steve Jessica Bob Dave Christine Rico Sara );
  # hash to resolve PID's back to child specific information

  my $pm = Parallel::ForkManager->new($max_procs);

  # Setup a callback for when a child finishes up so we can
  # get it's exit code
  $pm->run_on_finish( sub {
      my ($pid, $exit_code, $ident) = @_;
      print "** $ident just got out of the pool ".
        "with PID $pid and exit code: $exit_code\n";
  });

  $pm->run_on_start( sub {
      my ($pid, $ident)=@_;
      print "** $ident started, pid: $pid\n";
  });

  $pm->run_on_wait( sub {
      print "** Have to wait for one children ...\n"
    },
    0.5
  );

  NAMES:
  foreach my $child ( 0 .. $#names ) {
    my $pid = $pm->start($names[$child]) and next NAMES;

    # This code is the child process
    print "This is $names[$child], Child number $child\n";
    sleep ( 2 * $child );
    print "$names[$child], Child $child is about to get out...\n";
    sleep 1;
    $pm->finish($child); # pass an exit code to finish
  }

  print "Waiting for Children...\n";
  $pm->wait_all_children;
  print "Everybody is out of the pool!\n";

=head2 Data structure retrieval

In this simple example, each child sends back a string reference.

  use Parallel::ForkManager 0.7.6;
  use strict;

  my $pm = Parallel::ForkManager->new(2, '/server/path/to/temp/dir/');

  # data structure retrieval and handling
  $pm -> run_on_finish ( # called BEFORE the first call to start()
    sub {
      my ($pid, $exit_code, $ident, $exit_signal, $core_dump, $data_structure_reference) = @_;

      # retrieve data structure from child
      if (defined($data_structure_reference)) {  # children are not forced to send anything
        my $string = ${$data_structure_reference};  # child passed a string reference
        print "$string\n";
      }
      else {  # problems occurring during storage or retrieval will throw a warning
        print qq|No message received from child process $pid!\n|;
      }
    }
  );

  # prep random statement components
  my @foods = ('chocolate', 'ice cream', 'peanut butter', 'pickles', 'pizza', 'bacon', 'pancakes', 'spaghetti', 'cookies');
  my @preferences = ('loves', q|can't stand|, 'always wants more', 'will walk 100 miles for', 'only eats', 'would starve rather than eat');

  # run the parallel processes
  PERSONS:
  foreach my $person (qw(Fred Wilma Ernie Bert Lucy Ethel Curly Moe Larry)) {
    $pm->start() and next PERSONS;

    # generate a random statement about food preferences
    my $statement = $person . ' ' . $preferences[int(rand @preferences)] . ' ' . $foods[int(rand @foods)];

    # send it back to the parent process
    $pm->finish(0, \$statement);  # note that it's a scalar REFERENCE, not the scalar itself
  }
  $pm->wait_all_children;

A second datastructure retrieval example demonstrates how children decide
whether or not to send anything back, what to send and how the parent should
process whatever is retrieved.

=for example begin

  use Parallel::ForkManager 0.7.6;
  use Data::Dumper;  # to display the data structures retrieved.
  use strict;

  my $pm = Parallel::ForkManager->new(20);  # using the system temp dir $L<File::Temp::tempdir()

  # data structure retrieval and handling
  my %retrieved_responses = ();  # for collecting responses
  $pm -> run_on_finish (
    sub {
      my ($pid, $exit_code, $ident, $exit_signal, $core_dump, $data_structure_reference) = @_;

      # see what the child sent us, if anything
      if (defined($data_structure_reference)) {  # test rather than assume child sent anything
        my $reftype = ref($data_structure_reference);
        print qq|ident "$ident" returned a "$reftype" reference.\n\n|;
        if (1) {  # simple on/off switch to display the contents
          print &Dumper($data_structure_reference) . qq|end of "$ident" sent structure\n\n|;
        }

        # we can also collect retrieved data structures for processing after all children have exited
        $retrieved_responses{$ident} = $data_structure_reference;
      } else {
        print qq|ident "$ident" did not send anything.\n\n|;
      }
    }
  );

  # generate a list of instructions
  my @instructions = (  # a unique identifier and what the child process should send
    {'name' => '%ENV keys as a string', 'send' => 'keys'},
    {'name' => 'Send Nothing'},  # not instructing the child to send anything back to the parent
    {'name' => 'Childs %ENV', 'send' => 'all'},
    {'name' => 'Child chooses randomly', 'send' => 'random'},
    {'name' => 'Invalid send instructions', 'send' => 'Na Na Nana Na'},
    {'name' => 'ENV values in an array', 'send' => 'values'},
  );

  INSTRUCTS:
  foreach my $instruction (@instructions) {
    $pm->start($instruction->{'name'}) and next INSTRUCTS;  # this time we are using an explicit, unique child process identifier

    # last step in child processing
    $pm->finish(0) unless $instruction->{'send'};  # no data structure is sent unless this child is told what to send.

    if ($instruction->{'send'} eq 'keys') {
      $pm->finish(0, \join(', ', keys %ENV));

    } elsif ($instruction->{'send'} eq 'values') {
      $pm->finish(0, [values %ENV]);  # kinda useless without knowing which keys they belong to...

    } elsif ($instruction->{'send'} eq 'all') {
      $pm->finish(0, \%ENV);  # remember, we are not "returning" anything, just copying the hash to disc

    # demonstrate clearly that the child determines what type of reference to send
    } elsif ($instruction->{'send'} eq 'random') {
      my $string = q|I'm just a string.|;
      my @array = qw(I am an array);
      my %hash = (type => 'associative array', synonym => 'hash', cool => 'very :)');
      my $return_choice = ('string', 'array', 'hash')[int(rand 3)];  # randomly choose return data type
      $pm->finish(0, \$string) if ($return_choice eq 'string');
      $pm->finish(0, \@array) if ($return_choice eq 'array');
      $pm->finish(0, \%hash) if ($return_choice eq 'hash');

    # as a responsible child, inform parent that their instruction was invalid
    } else {
      $pm->finish(0, \qq|Invalid instructions: "$instruction->{'send'}".|);  # ordinarily I wouldn't include invalid input in a response...
    }
  }
  $pm->wait_all_children;  # blocks until all forked processes have exited

  # post fork processing of returned data structures
  for (sort keys %retrieved_responses) {
    print qq|Post processing "$_"...\n|;
  }

=for example end

=head1 SECURITY

Parallel::ForkManager uses temporary files when 
a child process returns information to its parent process. The filenames are
based on the process of the parent and child processes, so they are 
fairly easy to guess. So if security is a concern in your environment, make sure
the directory used by Parallel::ForkManager is restricted to the current user
only (the default behavior is to create a directory,
via L<File::Temp>'s C<tempdir>, which does that).

=head1 TROUBLESHOOTING

=head2 PerlIO::gzip and Parallel::ForkManager do not play nice together

If you are using L<PerlIO::gzip> in your child processes, you may end up with 
garbled files. This is not really P::FM's fault, but rather a problem between
L<PerlIO::gzip> and C<fork()> (see L<https://rt.cpan.org/Public/Bug/Display.html?id=114557>).

Fortunately, it seems there is an easy way to fix the problem by
adding the "unix" layer? I.e.,

    open(IN, '<:unix:gzip', ...

=head1 BUGS AND LIMITATIONS

Do not use Parallel::ForkManager in an environment, where other child
processes can affect the run of the main program, so using this module
is not recommended in an environment where fork() / wait() is already used.

If you want to use more than one copies of the Parallel::ForkManager, then
you have to make sure that all children processes are terminated, before you
use the second object in the main program.

You are free to use a new copy of Parallel::ForkManager in the child
processes, although I don't think it makes sense.

=head1 CREDITS

  Michael Gang (bug report)
  Noah Robin <sitz@onastick.net> (documentation tweaks)
  Chuck Hirstius <chirstius@megapathdsl.net> (callback exit status, example)
  Grant Hopwood <hopwoodg@valero.com> (win32 port)
  Mark Southern <mark_southern@merck.com> (bugfix)
  Ken Clarke <www.perlprogrammer.net>  (datastructure retrieval)

=head1 AUTHORS

=over 4

=item *

dLux (Szabó, Balázs) <dlux@dlux.hu>

=item *

Yanick Champoux <yanick@cpan.org>

=item *

Gabor Szabo <gabor@szabgab.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2000 by Balázs Szabó.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
