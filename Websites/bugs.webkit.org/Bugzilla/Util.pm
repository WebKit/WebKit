# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Util;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
@Bugzilla::Util::EXPORT = qw(trick_taint detaint_natural detaint_signed
                             html_quote url_quote xml_quote
                             css_class_quote html_light_quote
                             i_am_cgi i_am_webservice correct_urlbase remote_ip
                             validate_ip do_ssl_redirect_if_required use_attachbase
                             diff_arrays on_main_db
                             trim wrap_hard wrap_comment find_wrap_point
                             format_time validate_date validate_time datetime_from
                             is_7bit_clean bz_crypt generate_random_password
                             validate_email_syntax check_email_syntax clean_text
                             get_text template_var display_value disable_utf8
                             detect_encoding email_filter
                             join_activity_entries read_text write_text);

use Bugzilla::Constants;
use Bugzilla::RNG qw(irand);
use Bugzilla::Error;

use Date::Parse;
use Date::Format;
use Digest;
use Email::Address;
use List::Util qw(first);
use Scalar::Util qw(tainted blessed);
use Text::Wrap;
use Encode qw(encode decode resolve_alias);
use Encode::Guess;
use File::Basename qw(dirname);
use File::Temp qw(tempfile);

sub trick_taint {
    require Carp;
    Carp::confess("Undef to trick_taint") unless defined $_[0];
    my $match = $_[0] =~ /^(.*)$/s;
    $_[0] = $match ? $1 : undef;
    return (defined($_[0]));
}

sub detaint_natural {
    my $match = $_[0] =~ /^([0-9]+)$/;
    $_[0] = $match ? int($1) : undef;
    return (defined($_[0]));
}

sub detaint_signed {
    my $match = $_[0] =~ /^([-+]?[0-9]+)$/;
    # The "int()" call removes any leading plus sign.
    $_[0] = $match ? int($1) : undef;
    return (defined($_[0]));
}

# Bug 120030: Override html filter to obscure the '@' in user
#             visible strings.
# Bug 319331: Handle BiDi disruptions.
sub html_quote {
    my $var = shift;
    $var =~ s/&/&amp;/g;
    $var =~ s/</&lt;/g;
    $var =~ s/>/&gt;/g;
    $var =~ s/"/&quot;/g;
    # Obscure '@'.
    $var =~ s/\@/\&#64;/g;

    state $use_utf8 = Bugzilla->params->{'utf8'};

    if ($use_utf8) {
        # Remove control characters if the encoding is utf8.
        # Other multibyte encodings may be using this range; so ignore if not utf8.
        $var =~ s/(?![\t\r\n])[[:cntrl:]]//g;

        # Remove the following characters because they're
        # influencing BiDi:
        # --------------------------------------------------------
        # |Code  |Name                      |UTF-8 representation|
        # |------|--------------------------|--------------------|
        # |U+202a|Left-To-Right Embedding   |0xe2 0x80 0xaa      |
        # |U+202b|Right-To-Left Embedding   |0xe2 0x80 0xab      |
        # |U+202c|Pop Directional Formatting|0xe2 0x80 0xac      |
        # |U+202d|Left-To-Right Override    |0xe2 0x80 0xad      |
        # |U+202e|Right-To-Left Override    |0xe2 0x80 0xae      |
        # --------------------------------------------------------
        #
        # The following are characters influencing BiDi, too, but
        # they can be spared from filtering because they don't
        # influence more than one character right or left:
        # --------------------------------------------------------
        # |Code  |Name                      |UTF-8 representation|
        # |------|--------------------------|--------------------|
        # |U+200e|Left-To-Right Mark        |0xe2 0x80 0x8e      |
        # |U+200f|Right-To-Left Mark        |0xe2 0x80 0x8f      |
        # --------------------------------------------------------
        $var =~ tr/\x{202a}-\x{202e}//d;
    }
    return $var;
}

sub read_text {
    my ($filename) = @_;
    open my $fh, '<:encoding(utf-8)', $filename;
    local $/ = undef;
    my $content = <$fh>;
    close $fh;
    return $content;
}

sub write_text {
    my ($filename, $content) = @_;
    my ($tmp_fh, $tmp_filename) = tempfile('.tmp.XXXXXXXXXX',
        DIR    => dirname($filename),
        UNLINK => 0,
    );
    binmode $tmp_fh, ':encoding(utf-8)';
    print $tmp_fh $content;
    close $tmp_fh;
    # File::Temp tries for secure files, but File::Slurp used the umask.
    chmod(0666 & ~umask, $tmp_filename);
    rename $tmp_filename, $filename;
}

sub html_light_quote {
    my ($text) = @_;
    # admin/table.html.tmpl calls |FILTER html_light| many times.
    # There is no need to recreate the HTML::Scrubber object again and again.
    my $scrubber = Bugzilla->process_cache->{html_scrubber};

    # List of allowed HTML elements having no attributes.
    my @allow = qw(b strong em i u p br abbr acronym ins del cite code var
                   dfn samp kbd big small sub sup tt dd dt dl ul li ol
                   fieldset legend);

    if (!Bugzilla->feature('html_desc')) {
        my $safe = join('|', @allow);
        my $chr = chr(1);

        # First, escape safe elements.
        $text =~ s#<($safe)>#$chr$1$chr#go;
        $text =~ s#</($safe)>#$chr/$1$chr#go;
        # Now filter < and >.
        $text =~ s#<#&lt;#g;
        $text =~ s#>#&gt;#g;
        # Restore safe elements.
        $text =~ s#$chr/($safe)$chr#</$1>#go;
        $text =~ s#$chr($safe)$chr#<$1>#go;
        return $text;
    }
    elsif (!$scrubber) {
        # We can be less restrictive. We can accept elements with attributes.
        push(@allow, qw(a blockquote q span));

        # Allowed protocols.
        my $safe_protocols = join('|', SAFE_PROTOCOLS);
        my $protocol_regexp = qr{(^(?:$safe_protocols):|^[^:]+$)}i;

        # Deny all elements and attributes unless explicitly authorized.
        my @default = (0 => {
                             id    => 1,
                             name  => 1,
                             class => 1,
                             '*'   => 0, # Reject all other attributes.
                            }
                       );

        # Specific rules for allowed elements. If no specific rule is set
        # for a given element, then the default is used.
        my @rules = (a => {
                           href   => $protocol_regexp,
                           target => qr{^(?:_blank|_parent|_self|_top)$}i,
                           title  => 1,
                           id     => 1,
                           name   => 1,
                           class  => 1,
                           '*'    => 0, # Reject all other attributes.
                          },
                     blockquote => {
                                    cite => $protocol_regexp,
                                    id    => 1,
                                    name  => 1,
                                    class => 1,
                                    '*'  => 0, # Reject all other attributes.
                                   },
                     'q' => {
                             cite => $protocol_regexp,
                             id    => 1,
                             name  => 1,
                             class => 1,
                             '*'  => 0, # Reject all other attributes.
                          },
                    );

        Bugzilla->process_cache->{html_scrubber} = $scrubber =
          HTML::Scrubber->new(default => \@default,
                              allow   => \@allow,
                              rules   => \@rules,
                              comment => 0,
                              process => 0);
    }
    return $scrubber->scrub($text);
}

sub email_filter {
    my ($toencode) = @_;
    if (!Bugzilla->user->id) {
        my @emails = Email::Address->parse($toencode);
        if (scalar @emails) {
            my @hosts = map { quotemeta($_->host) } @emails;
            my $hosts_re = join('|', @hosts);
            $toencode =~ s/\@(?:$hosts_re)//g;
            return $toencode;
        }
    }
    return $toencode;
}

# This originally came from CGI.pm, by Lincoln D. Stein
sub url_quote {
    my ($toencode) = (@_);
    utf8::encode($toencode) # The below regex works only on bytes
        if Bugzilla->params->{'utf8'} && utf8::is_utf8($toencode);
    $toencode =~ s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

sub css_class_quote {
    my ($toencode) = (@_);
    $toencode =~ s#[ /]#_#g;
    $toencode =~ s/([^a-zA-Z0-9_\-.])/uc sprintf("&#x%x;",ord($1))/eg;
    return $toencode;
}

sub xml_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/\"/\&quot;/g;
    $var =~ s/\'/\&apos;/g;
    
    # the following nukes characters disallowed by the XML 1.0
    # spec, Production 2.2. 1.0 declares that only the following 
    # are valid:
    # (#x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF])
    $var =~ s/([\x{0001}-\x{0008}]|
               [\x{000B}-\x{000C}]|
               [\x{000E}-\x{001F}]|
               [\x{D800}-\x{DFFF}]|
               [\x{FFFE}-\x{FFFF}])//gx;
    return $var;
}

sub i_am_cgi {
    # I use SERVER_SOFTWARE because it's required to be
    # defined for all requests in the CGI spec.
    return exists $ENV{'SERVER_SOFTWARE'} ? 1 : 0;
}

sub i_am_webservice {
    my $usage_mode = Bugzilla->usage_mode;
    return $usage_mode == USAGE_MODE_XMLRPC
           || $usage_mode == USAGE_MODE_JSON
           || $usage_mode == USAGE_MODE_REST;
}

# This exists as a separate function from Bugzilla::CGI::redirect_to_https
# because we don't want to create a CGI object during XML-RPC calls
# (doing so can mess up XML-RPC).
sub do_ssl_redirect_if_required {
    return if !i_am_cgi();
    return if !Bugzilla->params->{'ssl_redirect'};

    my $sslbase = Bugzilla->params->{'sslbase'};
    
    # If we're already running under SSL, never redirect.
    return if uc($ENV{HTTPS} || '') eq 'ON';
    # Never redirect if there isn't an sslbase.
    return if !$sslbase;
    Bugzilla->cgi->redirect_to_https();
}

sub correct_urlbase {
    my $ssl = Bugzilla->params->{'ssl_redirect'};
    my $urlbase = Bugzilla->params->{'urlbase'};
    my $sslbase = Bugzilla->params->{'sslbase'};

    if (!$sslbase) {
        return $urlbase;
    }
    elsif ($ssl) {
        return $sslbase;
    }
    else {
        # Return what the user currently uses.
        return (uc($ENV{HTTPS} || '') eq 'ON') ? $sslbase : $urlbase;
    }
}

sub remote_ip {
    my $ip = $ENV{'REMOTE_ADDR'} || '127.0.0.1';
    my @proxies = split(/[\s,]+/, Bugzilla->params->{'inbound_proxies'});

    # If the IP address is one of our trusted proxies, then we look at
    # the X-Forwarded-For header to determine the real remote IP address.
    if ($ENV{'HTTP_X_FORWARDED_FOR'} && first { $_ eq $ip } @proxies) {
        my @ips = split(/[\s,]+/, $ENV{'HTTP_X_FORWARDED_FOR'});
        # This header can contain several IP addresses. We want the
        # IP address of the machine which connected to our proxies as
        # all other IP addresses may be fake or internal ones.
        # Note that this may block a whole external proxy, but we have
        # no way to determine if this proxy is malicious or trustable.
        foreach my $remote_ip (reverse @ips) {
            if (!first { $_ eq $remote_ip } @proxies) {
                # Keep the original IP address if the remote IP is invalid.
                $ip = validate_ip($remote_ip) || $ip;
                last;
            }
        }
    }
    return $ip;
}

sub validate_ip {
    my $ip = shift;
    return is_ipv4($ip) || is_ipv6($ip);
}

# Copied from Data::Validate::IP::is_ipv4().
sub is_ipv4 {
    my $ip = shift;
    return unless defined $ip;

    my @octets = $ip =~ /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
    return unless scalar(@octets) == 4;

    foreach my $octet (@octets) {
        return unless ($octet >= 0 && $octet <= 255 && $octet !~ /^0\d{1,2}$/);
    }

    # The IP address is valid and can now be detainted.
    return join('.', @octets);
}

# Copied from Data::Validate::IP::is_ipv6().
sub is_ipv6 {
    my $ip = shift;
    return unless defined $ip;

    # If there is a :: then there must be only one :: and the length
    # can be variable. Without it, the length must be 8 groups.
    my @chunks = split(':', $ip);

    # Need to check if the last chunk is an IPv4 address, if it is we
    # pop it off and exempt it from the normal IPv6 checking and stick
    # it back on at the end. If there is only one chunk and it's an IPv4
    # address, then it isn't an IPv6 address.
    my $ipv4;
    my $expected_chunks = 8;
    if (@chunks > 1 && is_ipv4($chunks[$#chunks])) {
        $ipv4 = pop(@chunks);
        $expected_chunks--;
    }

    my $empty = 0;
    # Workaround to handle trailing :: being valid.
    if ($ip =~ /[0-9a-f]{1,4}::$/) {
        $empty++;
    # Single trailing ':' is invalid.
    } elsif ($ip =~ /:$/) {
        return;
    }

    foreach my $chunk (@chunks) {
        return unless $chunk =~ /^[0-9a-f]{0,4}$/i;
        $empty++ if $chunk eq '';
    }
    # More than one :: block is bad, but if it starts with :: it will
    # look like two, so we need an exception.
    if ($empty == 2 && $ip =~ /^::/) {
        # This is ok
    } elsif ($empty > 1) {
        return;
    }

    push(@chunks, $ipv4) if $ipv4;
    # Need 8 chunks, or we need an empty section that could be filled
    # to represent the missing '0' sections.
    return unless (@chunks == $expected_chunks || @chunks < $expected_chunks && $empty);

    my $ipv6 = join(':', @chunks);
    # The IP address is valid and can now be detainted.
    trick_taint($ipv6);

    # Need to handle the exception of trailing :: being valid.
    return "${ipv6}::" if $ip =~ /::$/;
    return $ipv6;
}

sub use_attachbase {
    my $attachbase = Bugzilla->params->{'attachment_base'};
    return ($attachbase ne ''
            && $attachbase ne Bugzilla->params->{'urlbase'}
            && $attachbase ne Bugzilla->params->{'sslbase'}) ? 1 : 0;
}

sub diff_arrays {
    my ($old_ref, $new_ref, $attrib) = @_;
    $attrib ||= 'name';

    my (%counts, %pos);
    # We are going to alter the old array.
    my @old = @$old_ref;
    my $i = 0;

    # $counts{foo}-- means old, $counts{foo}++ means new.
    # If $counts{foo} becomes positive, then we are adding new items,
    # else we simply cancel one old existing item. Remaining items
    # in the old list have been removed.
    foreach (@old) {
        next unless defined $_;
        my $value = blessed($_) ? $_->$attrib : $_;
        $counts{$value}--;
        push @{$pos{$value}}, $i++;
    }
    my @added;
    foreach (@$new_ref) {
        next unless defined $_;
        my $value = blessed($_) ? $_->$attrib : $_;
        if (++$counts{$value} > 0) {
            # Ignore empty strings, but objects having an empty string
            # as attribute are fine.
            push(@added, $_) unless ($value eq '' && !blessed($_));
        }
        else {
            my $old_pos = shift @{$pos{$value}};
            $old[$old_pos] = undef;
        }
    }
    # Ignore canceled items as well as empty strings.
    my @removed = grep { defined $_ && $_ ne '' } @old;
    return (\@removed, \@added);
}

sub trim {
    my ($str) = @_;
    if ($str) {
      $str =~ s/^\s+//g;
      $str =~ s/\s+$//g;
    }
    return $str;
}

sub wrap_comment {
    my ($comment, $cols) = @_;
    my $wrappedcomment = "";

    # Use 'local', as recommended by Text::Wrap's perldoc.
#if WEBKIT_CHANGES
    local $Text::Wrap::columns = $cols || COMMENT_COLS_WRAP;
    # Make words that are longer than COMMENT_COLS_WRAP not wrap.
#endif // WEBKIT_CHANGES
    local $Text::Wrap::huge    = 'overflow';
    # Don't mess with tabs.
    local $Text::Wrap::unexpand = 0;

    # If the line starts with ">", don't wrap it. Otherwise, wrap.
    foreach my $line (split(/\r\n|\r|\n/, $comment)) {
      if ($line =~ qr/^>/) {
        $wrappedcomment .= ($line . "\n");
      }
      else {
        $wrappedcomment .= (wrap('', '', $line) . "\n");
      }
    }

    chomp($wrappedcomment); # Text::Wrap adds an extra newline at the end.
    return $wrappedcomment;
}

sub find_wrap_point {
    my ($string, $maxpos) = @_;
    if (!$string) { return 0 }
    if (length($string) < $maxpos) { return length($string) }
    my $wrappoint = rindex($string, ",", $maxpos); # look for comma
    if ($wrappoint <= 0) {  # can't find comma
        $wrappoint = rindex($string, " ", $maxpos); # look for space
        if ($wrappoint <= 0) {  # can't find space
            $wrappoint = rindex($string, "-", $maxpos); # look for hyphen
            if ($wrappoint <= 0) {  # can't find hyphen
                $wrappoint = $maxpos;  # just truncate it
            } else {
                $wrappoint++; # leave hyphen on the left side
            }
        }
    }
    return $wrappoint;
}

sub join_activity_entries {
    my ($field, $current_change, $new_change) = @_;
    # We need to insert characters as these were removed by old
    # LogActivityEntry code.

    return $new_change if $current_change eq '';

    # Buglists and see_also need the comma restored
    if ($field eq 'dependson' || $field eq 'blocked' || $field eq 'see_also') {
        if (substr($new_change, 0, 1) eq ',' || substr($new_change, 0, 1) eq ' ') {
            return $current_change . $new_change;
        } else {
            return $current_change . ', ' . $new_change;
        }
    }

    # Assume bug_file_loc contain a single url, don't insert a delimiter
    if ($field eq 'bug_file_loc') {
        return $current_change . $new_change;
    }

    # All other fields get a space unless the first character of the second
    # string is a comma or space
    if (substr($new_change, 0, 1) eq ',' || substr($new_change, 0, 1) eq ' ') {
        return $current_change . $new_change;
    } else {
        return $current_change . ' ' . $new_change;
    }
}

sub wrap_hard {
    my ($string, $columns) = @_;
    local $Text::Wrap::columns = $columns;
    local $Text::Wrap::unexpand = 0;
    local $Text::Wrap::huge = 'wrap';
    
    my $wrapped = wrap('', '', $string);
    chomp($wrapped);
    return $wrapped;
}

sub format_time {
    my ($date, $format, $timezone) = @_;

    # If $format is not set, try to guess the correct date format.
    if (!$format) {
        if (!ref $date
            && $date =~ /^(\d{4})[-\.](\d{2})[-\.](\d{2}) (\d{2}):(\d{2})(:(\d{2}))?$/) 
        {
            my $sec = $7;
            if (defined $sec) {
                $format = "%Y-%m-%d %T %Z";
            } else {
                $format = "%Y-%m-%d %R %Z";
            }
        } else {
            # Default date format. See DateTime for other formats available.
            $format = "%Y-%m-%d %R %Z";
        }
    }

    my $dt = ref $date ? $date : datetime_from($date, $timezone);
    $date = defined $dt ? $dt->strftime($format) : '';
    return trim($date);
}

sub datetime_from {
    my ($date, $timezone) = @_;

    # In the database, this is the "0" date.
    return undef if $date =~ /^0000/;

    my @time;
    # Most dates will be in this format, avoid strptime's generic parser
    if ($date =~ /^(\d{4})[\.-](\d{2})[\.-](\d{2})(?: (\d{2}):(\d{2}):(\d{2}))?$/) {
        @time = ($6, $5, $4, $3, $2 - 1, $1 - 1900, undef);
    }
    else {
        @time = strptime($date);
    }

    unless (scalar @time) {
        # If an unknown timezone is passed (such as MSK, for Moskow),
        # strptime() is unable to parse the date. We try again, but we first
        # remove the timezone.
        $date =~ s/\s+\S+$//;
        @time = strptime($date);
    }

    return undef if !@time;

    # strptime() counts years from 1900, except if they are older than 1901
    # in which case it returns the full year (so 1890 -> 1890, but 1984 -> 84,
    # and 3790 -> 1890). We make a guess and assume that 1100 <= year < 3000.
    $time[5] += 1900 if $time[5] < 1100;

    my %args = (
        year   => $time[5],
        # Months start from 0 (January).
        month  => $time[4] + 1,
        day    => $time[3],
        hour   => $time[2],
        minute => $time[1],
        # DateTime doesn't like fractional seconds.
        # Also, sometimes seconds are undef.
        second => defined($time[0]) ? int($time[0]) : undef,
        # If a timezone was specified, use it. Otherwise, use the
        # local timezone.
        time_zone => DateTime::TimeZone->offset_as_string($time[6])
                     || Bugzilla->local_timezone,
    );

    # If something wasn't specified in the date, it's best to just not
    # pass it to DateTime at all. (This is important for doing datetime_from
    # on the deadline field, which is usually just a date with no time.)
    foreach my $arg (keys %args) {
        delete $args{$arg} if !defined $args{$arg};
    }

    # This module takes time to load and is only used here, so we
    # |require| it here rather than |use| it.
    require DateTime;
    my $dt = new DateTime(\%args);

    # Now display the date using the given timezone,
    # or the user's timezone if none is given.
    $dt->set_time_zone($timezone || Bugzilla->user->timezone);
    return $dt;
}

sub bz_crypt {
    my ($password, $salt) = @_;

    my $algorithm;
    if (!defined $salt) {
        # If you don't use a salt, then people can create tables of
        # hashes that map to particular passwords, and then break your
        # hashing very easily if they have a large-enough table of common
        # (or even uncommon) passwords. So we generate a unique salt for
        # each password in the database, and then just prepend it to
        # the hash.
        $salt = generate_random_password(PASSWORD_SALT_LENGTH);
        $algorithm = PASSWORD_DIGEST_ALGORITHM;
    }

    # We append the algorithm used to the string. This is good because then
    # we can change the algorithm being used, in the future, without 
    # disrupting the validation of existing passwords. Also, this tells
    # us if a password is using the old "crypt" method of hashing passwords,
    # because the algorithm will be missing from the string.
    if ($salt =~ /{([^}]+)}$/) {
        $algorithm = $1;
    }

    # Wide characters cause crypt and Digest to die.
    if (Bugzilla->params->{'utf8'}) {
        utf8::encode($password) if utf8::is_utf8($password);
    }

    my $crypted_password;
    if (!$algorithm) {
        # Crypt the password.
        $crypted_password = crypt($password, $salt);
    }
    else {
        my $hasher = Digest->new($algorithm);
        # Newly created salts won't yet have a comma.
        ($salt) = $salt =~ /^([^,]+),?/;
        $hasher->add($password, $salt);
        $crypted_password = $salt . ',' . $hasher->b64digest . "{$algorithm}";
    }

    # Return the crypted password.
    return $crypted_password;
}

# If you want to understand the security of strings generated by this
# function, here's a quick formula that will help you estimate:
# We pick from 62 characters, which is close to 64, which is 2^6.
# So 8 characters is (2^6)^8 == 2^48 combinations. Just multiply 6
# by the number of characters you generate, and that gets you the equivalent
# strength of the string in bits.
sub generate_random_password {
    my $size = shift || 10; # default to 10 chars if nothing specified
    return join("", map{ ('0'..'9','a'..'z','A'..'Z')[irand 62] } (1..$size));
}

sub validate_email_syntax {
    my ($addr) = @_;
    my $match = Bugzilla->params->{'emailregexp'};
    my $email = $addr . Bugzilla->params->{'emailsuffix'};
    # This regexp follows RFC 2822 section 3.4.1.
    my $addr_spec = $Email::Address::addr_spec;
    # RFC 2822 section 2.1 specifies that email addresses must
    # be made of US-ASCII characters only.
    # Email::Address::addr_spec doesn't enforce this.
    # We set the max length to 127 to ensure addresses aren't truncated when
    # inserted into the tokens.eventdata field.
    if ($addr =~ /$match/
        && $email !~ /\P{ASCII}/
        && $email =~ /^$addr_spec$/
        && length($email) <= 127)
    {
        # We assume these checks to suffice to consider the address untainted.
        trick_taint($_[0]);
        return 1;
    }
    return 0;
}

sub check_email_syntax {
    my ($addr) = @_;

    unless (validate_email_syntax(@_)) {
        my $email = $addr . Bugzilla->params->{'emailsuffix'};
        ThrowUserError('illegal_email_address', { addr => $email });
    }
}

sub validate_date {
    my ($date) = @_;
    my $date2;

    # $ts is undefined if the parser fails.
    my $ts = str2time($date);
    if ($ts) {
        $date2 = time2str("%Y-%m-%d", $ts);

        $date =~ s/(\d+)-0*(\d+?)-0*(\d+?)/$1-$2-$3/; 
        $date2 =~ s/(\d+)-0*(\d+?)-0*(\d+?)/$1-$2-$3/;
    }
    my $ret = ($ts && $date eq $date2);
    return $ret ? 1 : 0;
}

sub validate_time {
    my ($time) = @_;
    my $time2;

    # $ts is undefined if the parser fails.
    my $ts = str2time($time);
    if ($ts) {
        $time2 = time2str("%H:%M:%S", $ts);
        if ($time =~ /^(\d{1,2}):(\d\d)(?::(\d\d))?$/) {
            $time = sprintf("%02d:%02d:%02d", $1, $2, $3 || 0);
        }
    }
    my $ret = ($ts && $time eq $time2);
    return $ret ? 1 : 0;
}

sub is_7bit_clean {
    return $_[0] !~ /[^\x20-\x7E\x0A\x0D]/;
}

sub clean_text {
    my $dtext = shift;
    if ($dtext) {
        # change control characters into a space
        $dtext =~ s/[\x00-\x1F\x7F]+/ /g;
    }
    return trim($dtext);
}

sub on_main_db (&) {
    my $code = shift;
    my $original_dbh = Bugzilla->dbh;
    Bugzilla->request_cache->{dbh} = Bugzilla->dbh_main;
    $code->();
    Bugzilla->request_cache->{dbh} = $original_dbh;
}

sub get_text {
    my ($name, $vars) = @_;
    my $template = Bugzilla->template_inner;
    $vars ||= {};
    $vars->{'message'} = $name;
    my $message;
    $template->process('global/message.txt.tmpl', $vars, \$message)
      || ThrowTemplateError($template->error());

    # Remove the indenting that exists in messages.html.tmpl.
    $message =~ s/^    //gm;
    return $message;
}

sub template_var {
    my $name = shift;
    my $request_cache = Bugzilla->request_cache;
    my $cache = $request_cache->{util_template_var} ||= {};
    my $lang = $request_cache->{template_current_lang}->[0] || '';
    return $cache->{$lang}->{$name} if defined $cache->{$lang};

    my $template = Bugzilla->template_inner($lang);
    my %vars;
    # Note: If we suddenly start needing a lot of template_var variables,
    # they should move into their own template, not field-descs.
    $template->process('global/field-descs.none.tmpl',
                       { vars => \%vars, in_template_var => 1 })
      || ThrowTemplateError($template->error());

    $cache->{$lang} = \%vars;
    return $vars{$name};
}

sub display_value {
    my ($field, $value) = @_;
    return template_var('value_descs')->{$field}->{$value} // $value;
}

sub disable_utf8 {
    if (Bugzilla->params->{'utf8'}) {
        binmode STDOUT, ':bytes'; # Turn off UTF8 encoding.
    }
}

use constant UTF8_ACCIDENTAL => qw(shiftjis big5-eten euc-kr euc-jp);

sub detect_encoding {
    my $data = shift;

    Bugzilla->feature('detect_charset')
      || ThrowUserError('feature_disabled', { feature => 'detect_charset' });

    require Encode::Detect::Detector;
    import Encode::Detect::Detector 'detect';

    my $encoding = detect($data);
    $encoding = resolve_alias($encoding) if $encoding;

    # Encode::Detect is bad at detecting certain charsets, but Encode::Guess
    # is better at them. Here's the details:

    # shiftjis, big5-eten, euc-kr, and euc-jp: (Encode::Detect
    # tends to accidentally mis-detect UTF-8 strings as being
    # these encodings.)
    if ($encoding && grep($_ eq $encoding, UTF8_ACCIDENTAL)) {
        $encoding = undef;
        my $decoder = guess_encoding($data, UTF8_ACCIDENTAL);
        $encoding = $decoder->name if ref $decoder;
    }

    # Encode::Detect sometimes mis-detects various ISO encodings as iso-8859-8,
    # or cp1255, but Encode::Guess can usually tell which one it is.
    if ($encoding && ($encoding eq 'iso-8859-8' || $encoding eq 'cp1255')) {
        my $decoded_as = _guess_iso($data, 'iso-8859-8', 
            # These are ordered this way because it gives the most 
            # accurate results.
            qw(cp1252 iso-8859-7 iso-8859-2));
        $encoding = $decoded_as if $decoded_as;
    }

    return $encoding;
}

# A helper for detect_encoding.
sub _guess_iso {
    my ($data, $versus, @isos) = (shift, shift, shift);

    my $encoding;
    foreach my $iso (@isos) {
        my $decoder = guess_encoding($data, ($iso, $versus));
        if (ref $decoder) {
            $encoding = $decoder->name if ref $decoder;
            last;
        }
    }
    return $encoding;
}

1;

__END__

=head1 NAME

Bugzilla::Util - Generic utility functions for bugzilla

=head1 SYNOPSIS

  use Bugzilla::Util;

  # Functions for dealing with variable tainting
  trick_taint($var);
  detaint_natural($var);
  detaint_signed($var);

  # Functions for quoting
  html_quote($var);
  url_quote($var);
  xml_quote($var);
  email_filter($var);

  # Functions that tell you about your environment
  my $is_cgi   = i_am_cgi();
  my $is_webservice = i_am_webservice();
  my $urlbase  = correct_urlbase();

  # Data manipulation
  ($removed, $added) = diff_arrays(\@old, \@new);

  # Functions for manipulating strings
  $val = trim(" abc ");
  $wrapped = wrap_comment($comment);

  # Functions for formatting time
  format_time($time);
  datetime_from($time, $timezone);

  # Cryptographic Functions
  $crypted_password = bz_crypt($password);
  $new_password = generate_random_password($password_length);

  # Validation Functions
  validate_email_syntax($email);
  check_email_syntax($email);
  validate_date($date);

  # DB-related functions
  on_main_db {
     ... code here ...
  };

=head1 DESCRIPTION

This package contains various utility functions which do not belong anywhere
else.

B<It is not intended as a general dumping group for something which
people feel might be useful somewhere, someday>. Do not add methods to this
package unless it is intended to be used for a significant number of files,
and it does not belong anywhere else.

=head1 FUNCTIONS

This package provides several types of routines:

=head2 Tainting

Several functions are available to deal with tainted variables. B<Use these
with care> to avoid security holes.

=over 4

=item C<trick_taint($val)>

Tricks perl into untainting a particular variable.

Use trick_taint() when you know that there is no way that the data
in a scalar can be tainted, but taint mode still bails on it.

B<WARNING!! Using this routine on data that really could be tainted defeats
the purpose of taint mode.  It should only be used on variables that have been
sanity checked in some way and have been determined to be OK.>

=item C<detaint_natural($num)>

This routine detaints a natural number. It returns a true value if the
value passed in was a valid natural number, else it returns false. You
B<MUST> check the result of this routine to avoid security holes.

=item C<detaint_signed($num)>

This routine detaints a signed integer. It returns a true value if the
value passed in was a valid signed integer, else it returns false. You
B<MUST> check the result of this routine to avoid security holes.

=back

=head2 Quoting

Some values may need to be quoted from perl. However, this should in general
be done in the template where possible.

=over 4

=item C<html_quote($val)>

Returns a value quoted for use in HTML, with &, E<lt>, E<gt>, E<34> and @ being
replaced with their appropriate HTML entities.  Also, Unicode BiDi controls are
deleted.

=item C<html_light_quote($val)>

Returns a string where only explicitly allowed HTML elements and attributes
are kept. All HTML elements and attributes not being in the whitelist are either
escaped (if HTML::Scrubber is not installed) or removed.

=item C<url_quote($val)>

Quotes characters so that they may be included as part of a url.

=item C<css_class_quote($val)>

Quotes characters so that they may be used as CSS class names. Spaces
and forward slashes are replaced by underscores.

=item C<xml_quote($val)>

This is similar to C<html_quote>, except that ' is escaped to &apos;. This
is kept separate from html_quote partly for compatibility with previous code
(for &apos;) and partly for future handling of non-ASCII characters.

=item C<email_filter>

Removes the hostname from email addresses in the string, if the user
currently viewing Bugzilla is logged out. If the user is logged-in,
this filter just returns the input string.

=back

=head2 Environment and Location

Functions returning information about your environment or location.

=over 4

=item C<i_am_cgi()>

Tells you whether or not you are being run as a CGI script in a web
server. For example, it would return false if the caller is running
in a command-line script.

=item C<i_am_webservice()>

Tells you whether or not the current usage mode is WebServices related
such as JSONRPC, XMLRPC, or REST.

=item C<correct_urlbase()>

Returns either the C<sslbase> or C<urlbase> parameter, depending on the
current setting for the C<ssl_redirect> parameter.

=item C<remote_ip()>

Returns the IP address of the remote client. If Bugzilla is behind
a trusted proxy, it will get the remote IP address by looking at the
X-Forwarded-For header.

=item C<validate_ip($ip)>

Returns the sanitized IP address if it is a valid IPv4 or IPv6 address,
else returns undef.

=item C<use_attachbase()>

Returns true if an alternate host is used to display attachments; false
otherwise.

=back

=head2 Data Manipulation

=over 4

=item C<diff_arrays(\@old, \@new)>

 Description: Takes two arrayrefs, and will tell you what it takes to 
              get from @old to @new.
 Params:      @old = array that you are changing from
              @new = array that you are changing to
 Returns:     A list of two arrayrefs. The first is a reference to an 
              array containing items that were removed from @old. The
              second is a reference to an array containing items
              that were added to @old. If both returned arrays are 
              empty, @old and @new contain the same values.

=back

=head2 String Manipulation

=over 4

=item C<trim($str)>

Removes any leading or trailing whitespace from a string. This routine does not
modify the existing string.

=item C<wrap_hard($string, $size)>

Wraps a string, so that a line is I<never> longer than C<$size>.
Returns the string, wrapped.

=item C<wrap_comment($comment)>

Takes a bug comment, and wraps it to the appropriate length. The length is
currently specified in C<Bugzilla::Constants::COMMENT_COLS_WRAP>. Lines beginning
with ">" are assumed to be quotes, and they will not be wrapped.

The intended use of this function is to wrap comments that are about to be
displayed or emailed. Generally, wrapped text should not be stored in the
database.

=item C<find_wrap_point($string, $maxpos)>

Search for a comma, a whitespace or a hyphen to split $string, within the first
$maxpos characters. If none of them is found, just split $string at $maxpos.
The search starts at $maxpos and goes back to the beginning of the string.

=item C<join_activity_entries($field, $current_change, $new_change)>

Joins two strings together so they appear as one. The field name is specified
as the method of joining the two strings depends on this. Returns the
combined string.

=item C<is_7bit_clean($str)>

Returns true is the string contains only 7-bit characters (ASCII 32 through 126,
ASCII 10 (LineFeed) and ASCII 13 (Carrage Return).

=item C<disable_utf8()>

Disable utf8 on STDOUT (and display raw data instead).

=item C<detect_encoding($str)>

Guesses what encoding a given data is encoded in, returning the canonical name
of the detected encoding (which may be different from the MIME charset 
specification).

=item C<clean_text($str)>
Returns the parameter "cleaned" by exchanging non-printable characters with spaces.
Specifically characters (ASCII 0 through 31) and (ASCII 127) will become ASCII 32 (Space).

=item C<get_text>

=over

=item B<Description>

This is a method of getting localized strings within Bugzilla code.
Use this when you don't want to display a whole template, you just
want a particular string.

It uses the F<global/message.txt.tmpl> template to return a string.

=item B<Params>

=over

=item C<$message> - The identifier for the message.

=item C<$vars> - A hashref. Any variables you want to pass to the template.

=back

=item B<Returns>

A string.

=back


=item C<template_var>

This is a method of getting the value of a variable from a template in
Perl code. The available variables are in the C<global/field-descs.none.tmpl>
template. Just pass in the name of the variable that you want the value of.


=back

=head2 Formatting Time

=over 4

=item C<format_time($time)>

Takes a time and converts it to the desired format and timezone.
If no format is given, the routine guesses the correct one and returns
an empty array if it cannot. If no timezone is given, the user's timezone
is used, as defined in their preferences.

This routine is mainly called from templates to filter dates, see
"FILTER time" in L<Bugzilla::Template>.

=item C<datetime_from($time, $timezone)>

Returns a DateTime object given a date string. If the string is not in some
valid date format that C<strptime> understands, we return C<undef>.

You can optionally specify a timezone for the returned date. If not
specified, defaults to the currently-logged-in user's timezone, or
the Bugzilla server's local timezone if there isn't a logged-in user.

=back

=head2 Cryptography

=over 4

=item C<bz_crypt($password, $salt)>

Takes a string and returns a hashed (encrypted) value for it, using a
random salt. An optional salt string may also be passed in.

Please always use this function instead of the built-in perl C<crypt>
function, when checking or setting a password. Bugzilla does not use
C<crypt>.

=begin undocumented

Random salts are generated because the alternative is usually
to use the first two characters of the password itself, and since
the salt appears in plaintext at the beginning of the encrypted
password string this has the effect of revealing the first two
characters of the password to anyone who views the encrypted version.

=end undocumented

=item C<generate_random_password($password_length)>

Returns an alphanumeric string with the specified length
(10 characters by default). Use this function to generate passwords
and tokens.

=back

=head2 Validation

=over 4

=item C<validate_email_syntax($email)>

Do a syntax checking for a legal email address and returns 1 if
the check is successful, else returns 0.
Untaints C<$email> if successful.

=item C<check_email_syntax($email)>

Do a syntax checking for a legal email address and throws an error
if the check fails.
Untaints C<$email> if successful.

=item C<validate_date($date)>

Make sure the date has the correct format and returns 1 if
the check is successful, else returns 0.

=back

=head2 Database

=over

=item C<on_main_db>

Runs a block of code always on the main DB. Useful for when you're inside
a subroutine and need to do some writes to the database, but don't know
if Bugzilla is currently using the shadowdb or not. Used like:

 on_main_db {
     my $dbh = Bugzilla->dbh;
     $dbh->do("INSERT ...");
 }

=back

=head1 B<Methods in need of POD>

=over

=item do_ssl_redirect_if_required

=item validate_time

=item is_ipv4

=item is_ipv6

=item display_value

=back
