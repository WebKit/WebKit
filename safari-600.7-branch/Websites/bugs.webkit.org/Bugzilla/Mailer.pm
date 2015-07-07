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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Bryce Nesbitt <bryce-mozilla@nextbus.com>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 Jacob Steenhagen <jake@actex.net>
#                 Matthew Tuck <matty@chariot.net.au>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 J. Paul Reed <preed@sigkill.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Byron Jones <bugzilla@glob.com.au>
#                 Frédéric Buclin <LpSolit@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Mailer;

use strict;

use base qw(Exporter);
@Bugzilla::Mailer::EXPORT = qw(MessageToMTA build_thread_marker);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

use Date::Format qw(time2str);

use Encode qw(encode);
use Encode::MIME::Header;
use Email::Address;
use Email::MIME;
# Loading this gives us encoding_set.
use Email::MIME::Modifier;
use Email::Send;

sub MessageToMTA {
    my ($msg) = (@_);
    my $method = Bugzilla->params->{'mail_delivery_method'};
    return if $method eq 'None';

    my $email;
    if (ref $msg) {
        $email = $msg;
    }
    else {
        # RFC 2822 requires us to have CRLF for our line endings and
        # Email::MIME doesn't do this for us. We use \015 (CR) and \012 (LF)
        # directly because Perl translates "\n" depending on what platform
        # you're running on. See http://perldoc.perl.org/perlport.html#Newlines
        # We check for multiple CRs because of this Template-Toolkit bug:
        # https://rt.cpan.org/Ticket/Display.html?id=43345
        $msg =~ s/(?:\015+)?\012/\015\012/msg;
        $email = new Email::MIME($msg);
    }

    # We add this header to mark the mail as "auto-generated" and
    # thus to hopefully avoid auto replies.
    $email->header_set('Auto-Submitted', 'auto-generated');

    $email->walk_parts(sub {
        my ($part) = @_;
        return if $part->parts > 1; # Top-level
        my $content_type = $part->content_type || '';
        if ($content_type !~ /;/) {
            my $body = $part->body;
            if (Bugzilla->params->{'utf8'}) {
                $part->charset_set('UTF-8');
                # encoding_set works only with bytes, not with utf8 strings.
                my $raw = $part->body_raw;
                if (utf8::is_utf8($raw)) {
                    utf8::encode($raw);
                    $part->body_set($raw);
                }
            }
            $part->encoding_set('quoted-printable') if !is_7bit_clean($body);
        }
    });

    # MIME-Version must be set otherwise some mailsystems ignore the charset
    $email->header_set('MIME-Version', '1.0') if !$email->header('MIME-Version');

    # Encode the headers correctly in quoted-printable
    foreach my $header ($email->header_names) {
        my @values = $email->header($header);
        # We don't recode headers that happen multiple times.
        next if scalar(@values) > 1;
        if (my $value = $values[0]) {
            if (Bugzilla->params->{'utf8'} && !utf8::is_utf8($value)) {
                utf8::decode($value);
            }

            # avoid excessive line wrapping done by Encode.
            local $Encode::Encoding{'MIME-Q'}->{'bpl'} = 998;

            my $encoded = encode('MIME-Q', $value);
            $email->header_set($header, $encoded);
        }
    }

    my $from = $email->header('From');

    my ($hostname, @args);
    if ($method eq "Sendmail") {
        if (ON_WINDOWS) {
            $Email::Send::Sendmail::SENDMAIL = SENDMAIL_EXE;
        }
        push @args, "-i";
        # We want to make sure that we pass *only* an email address.
        if ($from) {
            my ($email_obj) = Email::Address->parse($from);
            if ($email_obj) {
                my $from_email = $email_obj->address;
                push(@args, "-f$from_email") if $from_email;
            }
        }
        push(@args, "-ODeliveryMode=deferred")
            if !Bugzilla->params->{"sendmailnow"};
    }
    else {
        # Sendmail will automatically append our hostname to the From
        # address, but other mailers won't.
        my $urlbase = Bugzilla->params->{'urlbase'};
        $urlbase =~ m|//([^:/]+)[:/]?|;
        $hostname = $1;
        $from .= "\@$hostname" if $from !~ /@/;
        $email->header_set('From', $from);
        
        # Sendmail adds a Date: header also, but others may not.
        if (!defined $email->header('Date')) {
            $email->header_set('Date', time2str("%a, %e %b %Y %T %z", time()));
        }
    }

    if ($method eq "SMTP") {
        push @args, Host  => Bugzilla->params->{"smtpserver"},
                    username => Bugzilla->params->{"smtp_username"},
                    password => Bugzilla->params->{"smtp_password"},
                    Hello => $hostname, 
                    Debug => Bugzilla->params->{'smtp_debug'};
    }

    if ($method eq "Test") {
        my $filename = bz_locations()->{'datadir'} . '/mailer.testfile';
        open TESTFILE, '>>', $filename;
        # From - <date> is required to be a valid mbox file.
        print TESTFILE "\n\nFrom - " . $email->header('Date') . "\n" . $email->as_string;
        close TESTFILE;
    }
    else {
        # This is useful for both Sendmail and Qmail, so we put it out here.
        local $ENV{PATH} = SENDMAIL_PATH;
        my $mailer = Email::Send->new({ mailer => $method, 
                                        mailer_args => \@args });
        my $retval = $mailer->send($email);
        ThrowCodeError('mail_send_error', { msg => $retval, mail => $email })
            if !$retval;
    }
}

# Builds header suitable for use as a threading marker in email notifications
sub build_thread_marker {
    my ($bug_id, $user_id, $is_new) = @_;

    if (!defined $user_id) {
        $user_id = Bugzilla->user->id;
    }

    my $sitespec = '@' . Bugzilla->params->{'urlbase'};
    $sitespec =~ s/:\/\//\./; # Make the protocol look like part of the domain
    $sitespec =~ s/^([^:\/]+):(\d+)/$1/; # Remove a port number, to relocate
    if ($2) {
        $sitespec = "-$2$sitespec"; # Put the port number back in, before the '@'
    }

    my $threadingmarker;
    if ($is_new) {
        $threadingmarker = "Message-ID: <bug-$bug_id-$user_id$sitespec>";
    }
    else {
        $threadingmarker = "In-Reply-To: <bug-$bug_id-$user_id$sitespec>" .
                           "\nReferences: <bug-$bug_id-$user_id$sitespec>";
    }

    return $threadingmarker;
}

1;
