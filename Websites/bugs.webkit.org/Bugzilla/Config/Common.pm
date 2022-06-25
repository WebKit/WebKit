# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Common;

use 5.10.1;
use strict;
use warnings;

use Email::Address;
use Socket;

use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Field;
use Bugzilla::Group;
use Bugzilla::Status;

use parent qw(Exporter);
@Bugzilla::Config::Common::EXPORT =
    qw(check_multi check_numeric check_regexp check_url check_group
       check_sslbase check_priority check_severity check_platform
       check_opsys check_shadowdb check_urlbase check_webdotbase
       check_user_verify_class check_ip check_font_file
       check_mail_delivery_method check_notification check_utf8
       check_bug_status check_smtp_auth check_theschwartz_available
       check_maxattachmentsize check_email check_smtp_ssl
       check_comment_taggers_group check_smtp_server
);

# Checking functions for the various values

sub check_multi {
    my ($value, $param) = (@_);

    if ($param->{'type'} eq "s") {
        unless (scalar(grep {$_ eq $value} (@{$param->{'choices'}}))) {
            return "Invalid choice '$value' for single-select list param '$param->{'name'}'";
        }

        return "";
    }
    elsif ($param->{'type'} eq 'm' || $param->{'type'} eq 'o') {
        if (ref($value) ne "ARRAY") {
            $value = [split(',', $value)]
        }
        foreach my $chkParam (@$value) {
            unless (scalar(grep {$_ eq $chkParam} (@{$param->{'choices'}}))) {
                return "Invalid choice '$chkParam' for multi-select list param '$param->{'name'}'";
            }
        }

        return "";
    }
    else {
        return "Invalid param type '$param->{'type'}' for check_multi(); " .
          "contact your Bugzilla administrator";
    }
}

sub check_numeric {
    my ($value) = (@_);
    if ($value !~ /^[0-9]+$/) {
        return "must be a numeric value";
    }
    return "";
}

sub check_regexp {
    my ($value) = (@_);
    eval { qr/$value/ };
    return $@;
}

sub check_email {
    my ($value) = @_;
    if ($value !~ $Email::Address::mailbox) {
        return "must be a valid email address.";
    }
    return "";
}

sub check_sslbase {
    my $url = shift;
    if ($url ne '') {
        if ($url !~ m#^https://([^/]+).*/$#) {
            return "must be a legal URL, that starts with https and ends with a slash.";
        }
        my $host = $1;
        # Fall back to port 443 if for some reason getservbyname() fails.
        my $port = getservbyname('https', 'tcp') || 443;
        if ($host =~ /^(.+):(\d+)$/) {
            $host = $1;
            $port = $2;
        }
        local *SOCK;
        my $proto = getprotobyname('tcp');
        socket(SOCK, PF_INET, SOCK_STREAM, $proto);
        my $iaddr = inet_aton($host) || return "The host $host cannot be resolved";
        my $sin = sockaddr_in($port, $iaddr);
        if (!connect(SOCK, $sin)) {
            return "Failed to connect to $host:$port ($!); unable to enable SSL";
        }
        close(SOCK);
    }
    return "";
}

sub check_ip {
    my $inbound_proxies = shift;
    my @proxies = split(/[\s,]+/, $inbound_proxies);
    foreach my $proxy (@proxies) {
        validate_ip($proxy) || return "$proxy is not a valid IPv4 or IPv6 address";
    }
    return "";
}

sub check_utf8 {
    my $utf8 = shift;
    # You cannot turn off the UTF-8 parameter if you've already converted
    # your tables to utf-8.
    my $dbh = Bugzilla->dbh;
    if ($dbh->isa('Bugzilla::DB::Mysql') && $dbh->bz_db_is_utf8 && !$utf8) {
        return "You cannot disable UTF-8 support, because your MySQL database"
               . " is encoded in UTF-8";
    }
    return "";
}

sub check_priority {
    my ($value) = (@_);
    my $legal_priorities = get_legal_field_values('priority');
    if (!grep($_ eq $value, @$legal_priorities)) {
        return "Must be a legal priority value: one of " .
            join(", ", @$legal_priorities);
    }
    return "";
}

sub check_severity {
    my ($value) = (@_);
    my $legal_severities = get_legal_field_values('bug_severity');
    if (!grep($_ eq $value, @$legal_severities)) {
        return "Must be a legal severity value: one of " .
            join(", ", @$legal_severities);
    }
    return "";
}

sub check_platform {
    my ($value) = (@_);
    my $legal_platforms = get_legal_field_values('rep_platform');
    if (!grep($_ eq $value, '', @$legal_platforms)) {
        return "Must be empty or a legal platform value: one of " .
            join(", ", @$legal_platforms);
    }
    return "";
}

sub check_opsys {
    my ($value) = (@_);
    my $legal_OS = get_legal_field_values('op_sys');
    if (!grep($_ eq $value, '', @$legal_OS)) {
        return "Must be empty or a legal operating system value: one of " .
            join(", ", @$legal_OS);
    }
    return "";
}

sub check_bug_status {
    my $bug_status = shift;
    my @closed_bug_statuses = map {$_->name} closed_bug_statuses();
    if (!grep($_ eq $bug_status, @closed_bug_statuses)) {
        return "Must be a valid closed status: one of " . join(', ', @closed_bug_statuses);
    }
    return "";
}

sub check_group {
    my $group_name = shift;
    return "" unless $group_name;
    my $group = new Bugzilla::Group({'name' => $group_name});
    unless (defined $group) {
        return "Must be an existing group name";
    }
    return "";
}

sub check_shadowdb {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }

    if (!Bugzilla->params->{'shadowdbhost'}) {
        return "You need to specify a host when using a shadow database";
    }

    # Can't test existence of this because ConnectToDatabase uses the param,
    # but we can't set this before testing....
    # This can really only be fixed after we can use the DBI more openly
    return "";
}

sub check_urlbase {
    my ($url) = (@_);
    if ($url && $url !~ m:^http.*/$:) {
        return "must be a legal URL, that starts with http and ends with a slash.";
    }
    return "";
}

sub check_url {
    my ($url) = (@_);
    return '' if $url eq ''; # Allow empty URLs
    if ($url !~ m:/$:) {
        return 'must be a legal URL, absolute or relative, ending with a slash.';
    }
    return '';
}

sub check_webdotbase {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }
    if($value !~ /^https?:/) {
        if(! -x $value) {
            return "The file path \"$value\" is not a valid executable.  Please specify the complete file path to 'dot' if you intend to generate graphs locally.";
        }
        # Check .htaccess allows access to generated images
        my $webdotdir = bz_locations()->{'webdotdir'};
        if(-e "$webdotdir/.htaccess") {
            open HTACCESS, "<", "$webdotdir/.htaccess";
            if(! grep(/ \\\.png\$/,<HTACCESS>)) {
                return "Dependency graph images are not accessible.\nAssuming that you have not modified the file, delete $webdotdir/.htaccess and re-run checksetup.pl to rectify.\n";
            }
            close HTACCESS;
        }
    }
    return "";
}

sub check_font_file {
    my ($font) = @_;
    $font = trim($font);
    return '' unless $font;

    if ($font !~ /\.(ttf|otf)$/) {
        return "The file must point to a TrueType or OpenType font file (its extension must be .ttf or .otf)"
    }
    if (! -f $font) {
        return "The file '$font' cannot be found. Make sure you typed the full path to the file"
    }
    return '';
}

sub check_user_verify_class {
    # doeditparams traverses the list of params, and for each one it checks,
    # then updates. This means that if one param checker wants to look at 
    # other params, it must be below that other one. So you can't have two 
    # params mutually dependent on each other.
    # This means that if someone clears the LDAP config params after setting
    # the login method as LDAP, we won't notice, but all logins will fail.
    # So don't do that.

    my $params = Bugzilla->params;
    my ($list, $entry) = @_;
    $list || return 'You need to specify at least one authentication mechanism';
    for my $class (split /,\s*/, $list) {
        my $res = check_multi($class, $entry);
        return $res if $res;
        if ($class eq 'RADIUS') {
            if (!Bugzilla->feature('auth_radius')) {
                return "RADIUS support is not available. Run checksetup.pl"
                       . " for more details";
            }
            return "RADIUS servername (RADIUS_server) is missing"
                if !$params->{"RADIUS_server"};
            return "RADIUS_secret is empty" if !$params->{"RADIUS_secret"};
        }
        elsif ($class eq 'LDAP') {
            if (!Bugzilla->feature('auth_ldap')) {
                return "LDAP support is not available. Run checksetup.pl"
                       . " for more details";
            }
            return "LDAP servername (LDAPserver) is missing" 
                if !$params->{"LDAPserver"};
            return "LDAPBaseDN is empty" if !$params->{"LDAPBaseDN"};
        }
    }
    return "";
}

sub check_mail_delivery_method {
    my $check = check_multi(@_);
    return $check if $check;
    my $mailer = shift;
    if ($mailer eq 'Sendmail' and ON_WINDOWS) {
        # look for sendmail.exe 
        return "Failed to locate " . SENDMAIL_EXE
            unless -e SENDMAIL_EXE;
    }
    return "";
}

sub check_maxattachmentsize {
    my $check = check_numeric(@_);
    return $check if $check;
    my $size = shift;
    my $dbh = Bugzilla->dbh;
    if ($dbh->isa('Bugzilla::DB::Mysql')) {
        my (undef, $max_packet) = $dbh->selectrow_array(
            q{SHOW VARIABLES LIKE 'max\_allowed\_packet'});
        my $byte_size = $size * 1024;
        if ($max_packet < $byte_size) {
            return "You asked for a maxattachmentsize of $byte_size bytes,"
                   . " but the max_allowed_packet setting in MySQL currently"
                   . " only allows packets up to $max_packet bytes";
        }
    }
    return "";
}

sub check_notification {
    my $option = shift;
    my @current_version =
        (BUGZILLA_VERSION =~ m/^(\d+)\.(\d+)(?:(rc|\.)(\d+))?\+?$/);
    if ($current_version[1] % 2 && $option eq 'stable_branch_release') {
        return "You are currently running a development snapshot, and so your " .
               "installation is not based on a branch. If you want to be notified " .
               "about the next stable release, you should select " .
               "'latest_stable_release' instead";
    }
    if ($option ne 'disabled' && !Bugzilla->feature('updates')) {
        return "Some Perl modules are missing to get notifications about " .
               "new releases. See the output of checksetup.pl for more information";
    }
    return "";
}

sub check_smtp_server {
    my $host = shift;
    my $port;

    return '' unless $host;

    if ($host =~ /:/) {
        ($host, $port) = split(/:/, $host, 2);
        unless ($port && detaint_natural($port)) {
            return "Invalid port. It must be an integer (typically 25, 465 or 587)";
        }
    }
    trick_taint($host);
    # Let's first try to connect using SSL. If this fails, we fall back to
    # an unencrypted connection.
    foreach my $method (['Net::SMTP::SSL', 465], ['Net::SMTP', 25]) {
        my ($class, $default_port) = @$method;
        next if $class eq 'Net::SMTP::SSL' && !Bugzilla->feature('smtp_ssl');
        eval "require $class";
        my $smtp = $class->new($host, Port => $port || $default_port, Timeout => 5);
        if ($smtp) {
            # The connection works!
            $smtp->quit;
            return '';
        }
    }
    return "Cannot connect to $host" . ($port ? " using port $port" : "");
}

sub check_smtp_auth {
    my $username = shift;
    if ($username and !Bugzilla->feature('smtp_auth')) {
        return "SMTP Authentication is not available. Run checksetup.pl for"
               . " more details";
    }
    return "";
}

sub check_smtp_ssl {
    my $use_ssl = shift;
    if ($use_ssl && !Bugzilla->feature('smtp_ssl')) {
        return "SSL support is not available. Run checksetup.pl for more details";
    }
    return "";
}

sub check_theschwartz_available {
    my $use_queue = shift;
    if ($use_queue && !Bugzilla->feature('jobqueue')) {
        return "Using the job queue requires that you have certain Perl"
               . " modules installed. See the output of checksetup.pl"
               . " for more information";
    }
    return "";
}

sub check_comment_taggers_group {
    my $group_name = shift;
    if ($group_name && !Bugzilla->feature('jsonrpc')) {
        return "Comment tagging requires installation of the JSONRPC feature";
    }
    return check_group($group_name);
}

# OK, here are the parameter definitions themselves.
#
# Each definition is a hash with keys:
#
# name    - name of the param
# desc    - description of the param (for editparams.cgi)
# type    - see below
# choices - (optional) see below
# default - default value for the param
# checker - (optional) checking function for validating parameter entry
#           It is called with the value of the param as the first arg and a
#           reference to the param's hash as the second argument
#
# The type value can be one of the following:
#
# t -- A short text entry field (suitable for a single line)
# p -- A short text entry field (as with type = 't'), but the string is
#      replaced by asterisks (appropriate for passwords)
# l -- A long text field (suitable for many lines)
# b -- A boolean value (either 1 or 0)
# m -- A list of values, with many selectable (shows up as a select box)
#      To specify the list of values, make the 'choices' key be an array
#      reference of the valid choices. The 'default' key should be a string
#      with a list of selected values (as a comma-separated list), i.e.:
#       {
#         name => 'multiselect',
#         desc => 'A list of options, choose many',
#         type => 'm',
#         choices => [ 'a', 'b', 'c', 'd' ],
#         default => [ 'a', 'd' ],
#         checker => \&check_multi
#       }
#
#      Here, 'a' and 'd' are the default options, and the user may pick any
#      combination of a, b, c, and d as valid options.
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.
#
# o -- A list of values, orderable, and with many selectable (shows up as a
#      JavaScript-enhanced select box if JavaScript is enabled, and a text
#      entry field if not)
#      Set up in the same way as type m.
#
# s -- A list of values, with one selectable (shows up as a select box)
#      To specify the list of values, make the 'choices' key be an array
#      reference of the valid choices. The 'default' key should be one of
#      those values, i.e.:
#       {
#         name => 'singleselect',
#         desc => 'A list of options, choose one',
#         type => 's',
#         choices => [ 'a', 'b', 'c' ],
#         default => 'b',
#         checker => \&check_multi
#       }
#
#      Here, 'b' is the default option, and 'a' and 'c' are other possible
#      options, but only one at a time! 
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.

sub get_param_list {
    return;
}

1;

__END__

=head1 NAME

Bugzilla::Config::Common - Parameter checking functions

=head1 DESCRIPTION

All parameter checking functions are called with two parameters: the value to 
check, and a hash with the details of the param (type, default etc.) as defined
in the relevant F<Bugzilla::Config::*> package.

=head2 Functions

=over

=item C<check_multi>

Checks that a multi-valued parameter (ie types C<s>, C<o> or C<m>) satisfies
its contraints.

=item C<check_numeric>

Checks that the value is a valid number

=item C<check_regexp>

Checks that the value is a valid regexp

=item C<check_comment_taggers_group>

Checks that the required modules for comment tagging are installed, and that a
valid group is provided.

=back

=head1 B<Methods in need of POD>

=over

=item check_notification

=item check_priority

=item check_ip

=item check_user_verify_class

=item check_bug_status

=item check_shadowdb

=item check_smtp_server

=item check_smtp_auth

=item check_url

=item check_urlbase

=item check_email

=item check_webdotbase

=item check_font_file

=item get_param_list

=item check_maxattachmentsize

=item check_utf8

=item check_group

=item check_opsys

=item check_platform

=item check_severity

=item check_sslbase

=item check_mail_delivery_method

=item check_theschwartz_available

=item check_smtp_ssl

=back
