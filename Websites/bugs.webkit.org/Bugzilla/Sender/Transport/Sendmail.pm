# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Sender::Transport::Sendmail;

use 5.10.1;
use strict;
use warnings;

use parent qw(Email::Sender::Transport::Sendmail);

use Email::Sender::Failure;

sub send_email {
    my ($self, $email, $envelope) = @_;

    my $pipe = $self->_sendmail_pipe($envelope);

    my $string = $email->as_string;
    $string =~ s/\x0D\x0A/\x0A/g unless $^O eq 'MSWin32';

    print $pipe $string
      or Email::Sender::Failure->throw("couldn't send message to sendmail: $!");

    unless (close $pipe) {
        Email::Sender::Failure->throw("error when closing pipe to sendmail: $!") if $!;
        my ($error_message, $is_transient) = _map_exitcode($? >> 8);
        if (Bugzilla->params->{'use_mailer_queue'}) {
            # Return success for errors which are fatal so Bugzilla knows to
            # remove them from the queue.
            if ($is_transient) {
                Email::Sender::Failure->throw("error when closing pipe to sendmail: $error_message");
            } else {
                warn "error when closing pipe to sendmail: $error_message\n";
                return $self->success;
            }
        } else {
            Email::Sender::Failure->throw("error when closing pipe to sendmail: $error_message");
        }
    }
    return $self->success;
}

sub _map_exitcode {
    # Returns (error message, is_transient)
    # from the sendmail source (sendmail/sysexits.h)
    my $code = shift;
    if ($code == 64) {
        return ("Command line usage error (EX_USAGE)", 1);
    } elsif ($code == 65) {
        return ("Data format error (EX_DATAERR)", 1);
    } elsif ($code == 66) {
        return ("Cannot open input (EX_NOINPUT)", 1);
    } elsif ($code == 67) {
        return ("Addressee unknown (EX_NOUSER)", 0);
    } elsif ($code == 68) {
        return ("Host name unknown (EX_NOHOST)", 0);
    } elsif ($code == 69) {
        return ("Service unavailable (EX_UNAVAILABLE)", 1);
    } elsif ($code == 70) {
        return ("Internal software error (EX_SOFTWARE)", 1);
    } elsif ($code == 71) {
        return ("System error (EX_OSERR)", 1);
    } elsif ($code == 72) {
        return ("Critical OS file missing (EX_OSFILE)", 1);
    } elsif ($code == 73) {
        return ("Can't create output file (EX_CANTCREAT)", 1);
    } elsif ($code == 74) {
        return ("Input/output error (EX_IOERR)", 1);
    } elsif ($code == 75) {
        return ("Temp failure (EX_TEMPFAIL)", 1);
    } elsif ($code == 76) {
        return ("Remote error in protocol (EX_PROTOCOL)", 1);
    } elsif ($code == 77) {
        return ("Permission denied (EX_NOPERM)", 1);
    } elsif ($code == 78) {
        return ("Configuration error (EX_CONFIG)", 1);
    } else {
        return ("Unknown Error ($code)", 1);
    }
}

1;

=head1 B<Methods in need of POD>

=over

=item send_email

=back
