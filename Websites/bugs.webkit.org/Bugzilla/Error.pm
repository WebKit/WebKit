# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Error;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);

@Bugzilla::Error::EXPORT = qw(ThrowCodeError ThrowTemplateError ThrowUserError);

use Bugzilla::Constants;
use Bugzilla::WebService::Constants;
use Bugzilla::Hook;

use Carp;
use Data::Dumper;
use Date::Format;

# We cannot use $^S to detect if we are in an eval(), because mod_perl
# already eval'uates everything, so $^S = 1 in all cases under mod_perl!
sub _in_eval {
    my $in_eval = 0;
    for (my $stack = 1; my $sub = (caller($stack))[3]; $stack++) {
        last if $sub =~ /^ModPerl/;
        $in_eval = 1 if $sub =~ /^\(eval\)/;
    }
    return $in_eval;
}

sub _throw_error {
    my ($name, $error, $vars) = @_;
    my $dbh = Bugzilla->dbh;
    $vars ||= {};

    $vars->{error} = $error;

    # Make sure any transaction is rolled back (if supported).
    # If we are within an eval(), do not roll back transactions as we are
    # eval'uating some test on purpose.
    $dbh->bz_rollback_transaction() if ($dbh->bz_in_transaction() && !_in_eval());

    my $datadir = bz_locations()->{'datadir'};
    # If a writable $datadir/errorlog exists, log error details there.
    if (-w "$datadir/errorlog") {
        require Bugzilla::Util;
        require Data::Dumper;
        my $mesg = "";
        for (1..75) { $mesg .= "-"; };
        $mesg .= "\n[$$] " . time2str("%D %H:%M:%S ", time());
        $mesg .= "$name $error ";
        $mesg .= Bugzilla::Util::remote_ip();
        $mesg .= Bugzilla->user->login;
        $mesg .= (' actually ' . Bugzilla->sudoer->login) if Bugzilla->sudoer;
        $mesg .= "\n";
        my %params = Bugzilla->cgi->Vars;
        $Data::Dumper::Useqq = 1;
        for my $param (sort keys %params) {
            my $val = $params{$param};
            # obscure passwords
            $val = "*****" if $param =~ /password/i;
            # limit line length
            $val =~ s/^(.{512}).*$/$1\[CHOP\]/;
            $mesg .= "[$$] " . Data::Dumper->Dump([$val],["param($param)"]);
        }
        for my $var (sort keys %ENV) {
            my $val = $ENV{$var};
            $val = "*****" if $val =~ /password|http_pass/i;
            $mesg .= "[$$] " . Data::Dumper->Dump([$val],["env($var)"]);
        }
        open(ERRORLOGFID, ">>", "$datadir/errorlog");
        print ERRORLOGFID "$mesg\n";
        close ERRORLOGFID;
    }

    my $template = Bugzilla->template;
    my $message;
    # There are some tests that throw and catch a lot of errors,
    # and calling $template->process over and over for those errors
    # is too slow. So instead, we just "die" with a dump of the arguments.
    if (Bugzilla->error_mode != ERROR_MODE_TEST) {
        $template->process($name, $vars, \$message)
          || ThrowTemplateError($template->error());
    }

    # Let's call the hook first, so that extensions can override
    # or extend the default behavior, or add their own error codes.
    Bugzilla::Hook::process('error_catch', { error => $error, vars => $vars,
                                             message => \$message });

    if (Bugzilla->error_mode == ERROR_MODE_WEBPAGE) {
        my $cgi = Bugzilla->cgi;
        $cgi->close_standby_message('text/html', 'inline', 'error', 'html');
        print $message;
        print $cgi->multipart_final() if $cgi->{_multipart_in_progress};
    }
    elsif (Bugzilla->error_mode == ERROR_MODE_TEST) {
        die Dumper($vars);
    }
    elsif (Bugzilla->error_mode == ERROR_MODE_DIE) {
        die("$message\n");
    }
    elsif (Bugzilla->error_mode == ERROR_MODE_DIE_SOAP_FAULT
           || Bugzilla->error_mode == ERROR_MODE_JSON_RPC
           || Bugzilla->error_mode == ERROR_MODE_REST)
    {
        # Clone the hash so we aren't modifying the constant.
        my %error_map = %{ WS_ERROR_CODE() };
        Bugzilla::Hook::process('webservice_error_codes',
                                { error_map => \%error_map });
        my $code = $error_map{$error};
        if (!$code) {
            $code = ERROR_UNKNOWN_FATAL if $name =~ /code/i;
            $code = ERROR_UNKNOWN_TRANSIENT if $name =~ /user/i;
        }

        if (Bugzilla->error_mode == ERROR_MODE_DIE_SOAP_FAULT) {
            die SOAP::Fault->faultcode($code)->faultstring($message);
        }
        else {
            my $server = Bugzilla->_json_server;

            my $status_code = 0;
            if (Bugzilla->error_mode == ERROR_MODE_REST) {
                my %status_code_map = %{ REST_STATUS_CODE_MAP() };
                $status_code = $status_code_map{$code} || $status_code_map{'_default'};
            }
            # Technically JSON-RPC isn't allowed to have error numbers
            # higher than 999, but we do this to avoid conflicts with
            # the internal JSON::RPC error codes.
            $server->raise_error(code        => 100000 + $code,
                                 status_code => $status_code,
                                 message     => $message,
                                 id          => $server->{_bz_request_id},
                                 version     => $server->version);
            # Most JSON-RPC Throw*Error calls happen within an eval inside
            # of JSON::RPC. So, in that circumstance, instead of exiting,
            # we die with no message. JSON::RPC checks raise_error before
            # it checks $@, so it returns the proper error.
            die if _in_eval();
            $server->response($server->error_response_header);
        }
    }
    exit;
}

sub ThrowUserError {
    _throw_error("global/user-error.html.tmpl", @_);
}

sub ThrowCodeError {
    my (undef, $vars) = @_;

    # Don't show function arguments, in case they contain
    # confidential data.
    local $Carp::MaxArgNums = -1;
    # Don't show the error as coming from Bugzilla::Error, show it
    # as coming from the caller.
    local $Carp::CarpInternal{'Bugzilla::Error'} = 1;
    $vars->{traceback} = Carp::longmess();

    _throw_error("global/code-error.html.tmpl", @_);
}

sub ThrowTemplateError {
    my ($template_err) = @_;
    my $dbh = Bugzilla->dbh;

    # Make sure the transaction is rolled back (if supported).
    $dbh->bz_rollback_transaction() if $dbh->bz_in_transaction();

    my $vars = {};
    if (Bugzilla->error_mode == ERROR_MODE_DIE) {
        die("error: template error: $template_err");
    }

    $vars->{'template_error_msg'} = $template_err;
    $vars->{'error'} = "template_error";

    my $template = Bugzilla->template;

    # Try a template first; but if this one fails too, fall back
    # on plain old print statements.
    if (!$template->process("global/code-error.html.tmpl", $vars)) {
        require Bugzilla::Util;
        import Bugzilla::Util qw(html_quote);
        my $maintainer = Bugzilla->params->{'maintainer'};
        my $error = html_quote($vars->{'template_error_msg'});
        my $error2 = html_quote($template->error());
        my $url = html_quote(Bugzilla->cgi->self_url);

        print <<END;
          <p>
            Bugzilla has suffered an internal error. Please save this page and 
            send it to $maintainer with details of what you were doing at the 
            time this message appeared.
          </p>
          <p>URL: $url</p>
          <p>Template->process() failed twice.<br>
          First error: $error<br>
          Second error: $error2</p>
END
    }
    exit;
}

1;

__END__

=head1 NAME

Bugzilla::Error - Error handling utilities for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::Error;

  ThrowUserError("error_tag",
                 { foo => 'bar' });

=head1 DESCRIPTION

Various places throughout the Bugzilla codebase need to report errors to the
user. The C<Throw*Error> family of functions allow this to be done in a
generic and localizable manner.

These functions automatically unlock the database tables, if there were any
locked. They will also roll back the transaction, if it is supported by
the underlying DB.

=head1 FUNCTIONS

=over 4

=item C<ThrowUserError>

This function takes an error tag as the first argument, and an optional hashref
of variables as a second argument. These are used by the
I<global/user-error.html.tmpl> template to format the error, using the passed
in variables as required.

=item C<ThrowCodeError>

This function is used when an internal check detects an error of some sort.
This usually indicates a bug in Bugzilla, although it can occur if the user
manually constructs urls without correct parameters.

This function's behaviour is similar to C<ThrowUserError>, except that the
template used to display errors is I<global/code-error.html.tmpl>. In addition
if the hashref used as the optional second argument contains a key I<variables>
then the contents of the hashref (which is expected to be another hashref) will
be displayed after the error message, as a debugging aid.

=item C<ThrowTemplateError>

This function should only be called if a C<template-<gt>process()> fails.
It tries another template first, because often one template being
broken or missing doesn't mean that they all are. But it falls back to
a print statement as a last-ditch error.

=back

=head1 SEE ALSO

L<Bugzilla|Bugzilla>
