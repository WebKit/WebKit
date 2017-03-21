# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::MIME;

use 5.10.1;
use strict;
use warnings;

use parent qw(Email::MIME);

sub new {
    my ($class, $msg) = @_;
    state $use_utf8 = Bugzilla->params->{'utf8'};

    # Template-Toolkit trims trailing newlines, which is problematic when
    # parsing headers.
    $msg =~ s/\n*$/\n/;

    # Because the encoding headers are not present in our email templates, we
    # need to treat them as binary UTF-8 when parsing.
    my ($in_header, $has_type, $has_encoding, $has_body) = (1);
    foreach my $line (split(/\n/, $msg)) {
        if ($line eq '') {
            $in_header = 0;
            next;
        }
        if (!$in_header) {
            $has_body = 1;
            last;
        }
        $has_type = 1 if $line =~ /^Content-Type:/i;
        $has_encoding = 1 if $line =~ /^Content-Transfer-Encoding:/i;
    }
    if ($has_body) {
        if (!$has_type && $use_utf8) {
            $msg = qq#Content-Type: text/plain; charset="UTF-8"\n# . $msg;
        }
        if (!$has_encoding) {
            $msg = qq#Content-Transfer-Encoding: binary\n# . $msg;
        }
    }
    if ($use_utf8 && utf8::is_utf8($msg)) {
        utf8::encode($msg);
    }

    # RFC 2822 requires us to have CRLF for our line endings and
    # Email::MIME doesn't do this for us. We use \015 (CR) and \012 (LF)
    # directly because Perl translates "\n" depending on what platform
    # you're running on. See http://perldoc.perl.org/perlport.html#Newlines
    $msg =~ s/(?:\015+)?\012/\015\012/msg;

    return $class->SUPER::new($msg);
}

sub as_string {
    my $self = shift;
    state $use_utf8 = Bugzilla->params->{'utf8'};

    # We add this header to uniquely identify all email that we
    # send as coming from this Bugzilla installation.
    #
    # We don't use correct_urlbase, because we want this URL to
    # *always* be the same for this Bugzilla, in every email,
    # even if the admin changes the "ssl_redirect" parameter some day.
    $self->header_set('X-Bugzilla-URL', Bugzilla->params->{'urlbase'});

    # We add this header to mark the mail as "auto-generated" and
    # thus to hopefully avoid auto replies.
    $self->header_set('Auto-Submitted', 'auto-generated');

    # MIME-Version must be set otherwise some mailsystems ignore the charset
    $self->header_set('MIME-Version', '1.0') if !$self->header('MIME-Version');

    # Encode the headers correctly.
    foreach my $header ($self->header_names) {
        my @values = $self->header($header);
        map { utf8::decode($_) if defined($_) && !utf8::is_utf8($_) } @values;

        $self->header_str_set($header, @values);
    }

    # Ensure the character-set and encoding is set correctly on single part
    # emails.  Multipart emails should have these already set when the parts
    # are assembled.
    if (scalar($self->parts) == 1) {
        $self->charset_set('UTF-8') if $use_utf8;
        $self->encoding_set('quoted-printable');
    }

    # Ensure we always return the encoded string
    my $value = $self->SUPER::as_string();
    if ($use_utf8 && utf8::is_utf8($value)) {
        utf8::encode($value);
    }

    return $value;
}

1;

__END__

=head1 NAME

Bugzilla::MIME - Wrapper around Email::MIME for unifying MIME related
workarounds.

=head1 SYNOPSIS

  use Bugzilla::MIME;
  my $email = Bugzilla::MIME->new($message);

=head1 DESCRIPTION

Bugzilla::MIME subclasses Email::MIME and performs various fixes when parsing
and generating email.
