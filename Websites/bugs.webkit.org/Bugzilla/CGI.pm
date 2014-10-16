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
# Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Byron Jones <bugzilla@glob.com.au>
#                 Marc Schumann <wurblzap@gmail.com>

package Bugzilla::CGI;
use strict;
use base qw(CGI);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Search::Recent;

use File::Basename;

BEGIN {
    if (ON_WINDOWS) {
        # Help CGI find the correct temp directory as the default list
        # isn't Windows friendly (Bug 248988)
        $ENV{'TMPDIR'} = $ENV{'TEMP'} || $ENV{'TMP'} || "$ENV{'WINDIR'}\\TEMP";
    }
    *AUTOLOAD = \&CGI::AUTOLOAD;
}

sub _init_bz_cgi_globals {
    my $invocant = shift;
    # We need to disable output buffering - see bug 179174
    $| = 1;

    # Ignore SIGTERM and SIGPIPE - this prevents DB corruption. If the user closes
    # their browser window while a script is running, the web server sends these
    # signals, and we don't want to die half way through a write.
    $SIG{TERM} = 'IGNORE';
    $SIG{PIPE} = 'IGNORE';

    # We don't precompile any functions here, that's done specially in
    # mod_perl code.
    $invocant->_setup_symbols(qw(:no_xhtml :oldstyle_urls :private_tempfiles
                                 :unique_headers));
}

BEGIN { __PACKAGE__->_init_bz_cgi_globals() if i_am_cgi(); }

sub new {
    my ($invocant, @args) = @_;
    my $class = ref($invocant) || $invocant;

    # Under mod_perl, CGI's global variables get reset on each request,
    # so we need to set them up again every time.
    $class->_init_bz_cgi_globals() if $ENV{MOD_PERL};

    my $self = $class->SUPER::new(@args);

    # Make sure our outgoing cookie list is empty on each invocation
    $self->{Bugzilla_cookie_list} = [];

    # Send appropriate charset
    $self->charset(Bugzilla->params->{'utf8'} ? 'UTF-8' : '');

    # Redirect to urlbase/sslbase if we are not viewing an attachment.
    my $script = basename($0);
    if ($self->url_is_attachment_base and $script ne 'attachment.cgi') {
        $self->redirect_to_urlbase();
    }

    # Check for errors
    # All of the Bugzilla code wants to do this, so do it here instead of
    # in each script

    my $err = $self->cgi_error;

    if ($err) {
        # Note that this error block is only triggered by CGI.pm for malformed
        # multipart requests, and so should never happen unless there is a
        # browser bug.

        print $self->header(-status => $err);

        # ThrowCodeError wants to print the header, so it grabs Bugzilla->cgi
        # which creates a new Bugzilla::CGI object, which fails again, which
        # ends up here, and calls ThrowCodeError, and then recurses forever.
        # So don't use it.
        # In fact, we can't use templates at all, because we need a CGI object
        # to determine the template lang as well as the current url (from the
        # template)
        # Since this is an internal error which indicates a severe browser bug,
        # just die.
        die "CGI parsing error: $err";
    }

    return $self;
}

# We want this sorted plus the ability to exclude certain params
sub canonicalise_query {
    my ($self, @exclude) = @_;

    # Reconstruct the URL by concatenating the sorted param=value pairs
    my @parameters;
    foreach my $key (sort($self->param())) {
        # Leave this key out if it's in the exclude list
        next if grep { $_ eq $key } @exclude;

        # Remove the Boolean Charts for standard query.cgi fields
        # They are listed in the query URL already
        next if $key =~ /^(field|type|value)(-\d+){3}$/;

        my $esc_key = url_quote($key);

        foreach my $value ($self->param($key)) {
            if (defined($value)) {
                my $esc_value = url_quote($value);

                push(@parameters, "$esc_key=$esc_value");
            }
        }
    }

    return join("&", @parameters);
}

sub clean_search_url {
    my $self = shift;
    # Delete any empty URL parameter.
    my @cgi_params = $self->param;

    foreach my $param (@cgi_params) {
        if (defined $self->param($param) && $self->param($param) eq '') {
            $self->delete($param);
            $self->delete("${param}_type");
        }

        # Custom Search stuff is empty if it's "noop". We also keep around
        # the old Boolean Chart syntax for backwards-compatibility.
        if (($param =~ /\d-\d-\d/ || $param =~ /^[[:alpha:]]\d+$/)
            && defined $self->param($param) && $self->param($param) eq 'noop')
        {
            $self->delete($param);
        }
        
        # Any "join" for custom search that's an AND can be removed, because
        # that's the default.
        if (($param =~ /^j\d+$/ || $param eq 'j_top')
            && $self->param($param) eq 'AND')
        {
            $self->delete($param);
        }
    }

    # Delete leftovers from the login form
    $self->delete('Bugzilla_remember', 'GoAheadAndLogIn');

    # Delete the token if we're not performing an action which needs it
    unless ((defined $self->param('remtype')
             && ($self->param('remtype') eq 'asdefault'
                 || $self->param('remtype') eq 'asnamed'))
            || (defined $self->param('remaction')
                && $self->param('remaction') eq 'forget'))
    {
        $self->delete("token");
    }

    foreach my $num (1,2,3) {
        # If there's no value in the email field, delete the related fields.
        if (!$self->param("email$num")) {
            foreach my $field (qw(type assigned_to reporter qa_contact cc longdesc)) {
                $self->delete("email$field$num");
            }
        }
    }

    # chfieldto is set to "Now" by default in query.cgi. But if none
    # of the other chfield parameters are set, it's meaningless.
    if (!defined $self->param('chfieldfrom') && !$self->param('chfield')
        && !defined $self->param('chfieldvalue') && $self->param('chfieldto')
        && lc($self->param('chfieldto')) eq 'now')
    {
        $self->delete('chfieldto');
    }

    # cmdtype "doit" is the default from query.cgi, but it's only meaningful
    # if there's a remtype parameter.
    if (defined $self->param('cmdtype') && $self->param('cmdtype') eq 'doit'
        && !defined $self->param('remtype'))
    {
        $self->delete('cmdtype');
    }

    # "Reuse same sort as last time" is actually the default, so we don't
    # need it in the URL.
    if ($self->param('order') 
        && $self->param('order') eq 'Reuse same sort as last time')
    {
        $self->delete('order');
    }

    # list_id is added in buglist.cgi after calling clean_search_url,
    # and doesn't need to be saved in saved searches.
    $self->delete('list_id'); 

    # And now finally, if query_format is our only parameter, that
    # really means we have no parameters, so we should delete query_format.
    if ($self->param('query_format') && scalar($self->param()) == 1) {
        $self->delete('query_format');
    }
}

# Overwrite to ensure nph doesn't get set, and unset HEADERS_ONCE
sub multipart_init {
    my $self = shift;

    # Keys are case-insensitive, map to lowercase
    my %args = @_;
    my %param;
    foreach my $key (keys %args) {
        $param{lc $key} = $args{$key};
    }

    # Set the MIME boundary and content-type
    my $boundary = $param{'-boundary'}
        || '------- =_' . generate_random_password(16);
    delete $param{'-boundary'};
    $self->{'separator'} = "\r\n--$boundary\r\n";
    $self->{'final_separator'} = "\r\n--$boundary--\r\n";
    $param{'-type'} = SERVER_PUSH($boundary);

    # Note: CGI.pm::multipart_init up to v3.04 explicitly set nph to 0
    # CGI.pm::multipart_init v3.05 explicitly sets nph to 1
    # CGI.pm's header() sets nph according to a param or $CGI::NPH, which
    # is the desired behaviour.

    return $self->header(
        %param,
    ) . "WARNING: YOUR BROWSER DOESN'T SUPPORT THIS SERVER-PUSH TECHNOLOGY." . $self->multipart_end;
}

# Have to add the cookies in.
sub multipart_start {
    my $self = shift;
    
    my %args = @_;

    # CGI.pm::multipart_start doesn't honour its own charset information, so
    # we do it ourselves here
    if (defined $self->charset() && defined $args{-type}) {
        # Remove any existing charset specifier
        $args{-type} =~ s/;.*$//;
        # and add the specified one
        $args{-type} .= '; charset=' . $self->charset();
    }
        
    my $headers = $self->SUPER::multipart_start(%args);
    # Eliminate the one extra CRLF at the end.
    $headers =~ s/$CGI::CRLF$//;
    # Add the cookies. We have to do it this way instead of
    # passing them to multpart_start, because CGI.pm's multipart_start
    # doesn't understand a '-cookie' argument pointing to an arrayref.
    foreach my $cookie (@{$self->{Bugzilla_cookie_list}}) {
        $headers .= "Set-Cookie: ${cookie}${CGI::CRLF}";
    }
    $headers .= $CGI::CRLF;
    return $headers;
}

# Override header so we can add the cookies in
sub header {
    my $self = shift;

    # If there's only one parameter, then it's a Content-Type.
    if (scalar(@_) == 1) {
        # Since we're adding parameters below, we have to name it.
        unshift(@_, '-type' => shift(@_));
    }

    # Add the cookies in if we have any
    if (scalar(@{$self->{Bugzilla_cookie_list}})) {
        unshift(@_, '-cookie' => $self->{Bugzilla_cookie_list});
    }

    # Add Strict-Transport-Security (STS) header if this response
    # is over SSL and the strict_transport_security param is turned on.
    if ($self->https && !$self->url_is_attachment_base
        && Bugzilla->params->{'strict_transport_security'} ne 'off') 
    {
        my $sts_opts = 'max-age=' . MAX_STS_AGE;
        if (Bugzilla->params->{'strict_transport_security'} 
            eq 'include_subdomains')
        {
            $sts_opts .= '; includeSubDomains';
        }
        unshift(@_, '-strict_transport_security' => $sts_opts);
    }

    # Add X-Frame-Options header to prevent framing and subsequent
    # possible clickjacking problems.
    unless ($self->url_is_attachment_base) {
        unshift(@_, '-x_frame_options' => 'SAMEORIGIN');
    }

    # Add X-XSS-Protection header to prevent simple XSS attacks
    # and enforce the blocking (rather than the rewriting) mode.
    unshift(@_, '-x_xss_protection' => '1; mode=block');

    # Add X-Content-Type-Options header to prevent browsers sniffing
    # the MIME type away from the declared Content-Type.
    unshift(@_, '-x_content_type_options' => 'nosniff');

    return $self->SUPER::header(@_) || "";
}

sub param {
    my $self = shift;

    # When we are just requesting the value of a parameter...
    if (scalar(@_) == 1) {
        my @result = $self->SUPER::param(@_); 

        # Also look at the URL parameters, after we look at the POST 
        # parameters. This is to allow things like login-form submissions
        # with URL parameters in the form's "target" attribute.
        if (!scalar(@result)
            && $self->request_method && $self->request_method eq 'POST')
        {
            # Some servers fail to set the QUERY_STRING parameter, which
            # causes undef issues
            $ENV{'QUERY_STRING'} = '' unless exists $ENV{'QUERY_STRING'};
            @result = $self->SUPER::url_param(@_);
        }

        # Fix UTF-8-ness of input parameters.
        if (Bugzilla->params->{'utf8'}) {
            @result = map { _fix_utf8($_) } @result;
        }

        return wantarray ? @result : $result[0];
    }
    # And for various other functions in CGI.pm, we need to correctly
    # return the URL parameters in addition to the POST parameters when
    # asked for the list of parameters.
    elsif (!scalar(@_) && $self->request_method 
           && $self->request_method eq 'POST') 
    {
        my @post_params = $self->SUPER::param;
        my @url_params  = $self->url_param;
        my %params = map { $_ => 1 } (@post_params, @url_params);
        return keys %params;
    }

    return $self->SUPER::param(@_);
}

sub _fix_utf8 {
    my $input = shift;
    # The is_utf8 is here in case CGI gets smart about utf8 someday.
    utf8::decode($input) if defined $input && !ref $input && !utf8::is_utf8($input);
    return $input;
}

sub should_set {
    my ($self, $param) = @_;
    my $set = (defined $self->param($param) 
               or defined $self->param("defined_$param"))
              ? 1 : 0;
    return $set;
}

# The various parts of Bugzilla which create cookies don't want to have to
# pass them around to all of the callers. Instead, store them locally here,
# and then output as required from |header|.
sub send_cookie {
    my $self = shift;

    # Move the param list into a hash for easier handling.
    my %paramhash;
    my @paramlist;
    my ($key, $value);
    while ($key = shift) {
        $value = shift;
        $paramhash{$key} = $value;
    }

    # Complain if -value is not given or empty (bug 268146).
    if (!exists($paramhash{'-value'}) || !$paramhash{'-value'}) {
        ThrowCodeError('cookies_need_value');
    }

    # Add the default path and the domain in.
    $paramhash{'-path'} = Bugzilla->params->{'cookiepath'};
    $paramhash{'-domain'} = Bugzilla->params->{'cookiedomain'}
        if Bugzilla->params->{'cookiedomain'};

    # Move the param list back into an array for the call to cookie().
    foreach (keys(%paramhash)) {
        unshift(@paramlist, $_ => $paramhash{$_});
    }

    push(@{$self->{'Bugzilla_cookie_list'}}, $self->cookie(@paramlist));
}

# Cookies are removed by setting an expiry date in the past.
# This method is a send_cookie wrapper doing exactly this.
sub remove_cookie {
    my $self = shift;
    my ($cookiename) = (@_);

    # Expire the cookie, giving a non-empty dummy value (bug 268146).
    $self->send_cookie('-name'    => $cookiename,
                       '-expires' => 'Tue, 15-Sep-1998 21:49:00 GMT',
                       '-value'   => 'X');
}

# This helps implement Bugzilla::Search::Recent, and also shortens search
# URLs that get POSTed to buglist.cgi.
sub redirect_search_url {
    my $self = shift;
    # If we're retreiving an old list, we never need to redirect or
    # do anything related to Bugzilla::Search::Recent.
    return if $self->param('regetlastlist');

    my $user = Bugzilla->user;

    if ($user->id) {
        # There are two conditions that could happen here--we could get a URL
        # with no list id, and we could get a URL with a list_id that isn't
        # ours.
        my $list_id = $self->param('list_id');
        if ($list_id) {
            # If we have a valid list_id, no need to redirect or clean.
            return if Bugzilla::Search::Recent->check_quietly(
                { id => $list_id });
        }
    }
    elsif ($self->request_method ne 'POST') {
        # Logged-out users who do a GET don't get a list_id, don't get
        # their URLs cleaned, and don't get redirected.
        return;
    }

    $self->clean_search_url();

    # Make sure we still have params still after cleaning otherwise we 
    # do not want to store a list_id for an empty search.
    if ($user->id && $self->param) {
        # Insert a placeholder Bugzilla::Search::Recent, so that we know what
        # the id of the resulting search will be. This is then pulled out
        # of the Referer header when viewing show_bug.cgi to know what
        # bug list we came from.
        my $recent_search = Bugzilla::Search::Recent->create_placeholder;
        $self->param('list_id', $recent_search->id);
    }

    # GET requests that lacked a list_id are always redirected. POST requests
    # are only redirected if they're under the CGI_URI_LIMIT though.
    my $uri_length = length($self->self_url());
    if ($self->request_method() ne 'POST' or $uri_length < CGI_URI_LIMIT) {
        print $self->redirect(-url => $self->self_url());
        exit;
    }
}

sub redirect_to_https {
    my $self = shift;
    my $sslbase = Bugzilla->params->{'sslbase'};
    # If this is a POST, we don't want ?POSTDATA in the query string.
    # We expect the client to re-POST, which may be a violation of
    # the HTTP spec, but the only time we're expecting it often is
    # in the WebService, and WebService clients usually handle this
    # correctly.
    $self->delete('POSTDATA');
    my $url = $sslbase . $self->url('-path_info' => 1, '-query' => 1, 
                                    '-relative' => 1);

    # XML-RPC clients (SOAP::Lite at least) require a 301 to redirect properly
    # and do not work with 302. Our redirect really is permanent anyhow, so
    # it doesn't hurt to make it a 301.
    print $self->redirect(-location => $url, -status => 301);

    # When using XML-RPC with mod_perl, we need the headers sent immediately.
    $self->r->rflush if $ENV{MOD_PERL};
    exit;
}

# Redirect to the urlbase version of the current URL.
sub redirect_to_urlbase {
    my $self = shift;
    my $path = $self->url('-path_info' => 1, '-query' => 1, '-relative' => 1);
    print $self->redirect('-location' => correct_urlbase() . $path);
    exit;
}

sub url_is_attachment_base {
    my ($self, $id) = @_;
    return 0 if !use_attachbase() or !i_am_cgi();
    my $attach_base = Bugzilla->params->{'attachment_base'};
    # If we're passed an id, we only want one specific attachment base
    # for a particular bug. If we're not passed an ID, we just want to
    # know if our current URL matches the attachment_base *pattern*.
    my $regex;
    if ($id) {
        $attach_base =~ s/\%bugid\%/$id/;
        $regex = quotemeta($attach_base);
    }
    else {
        # In this circumstance we run quotemeta first because we need to
        # insert an active regex meta-character afterward.
        $regex = quotemeta($attach_base);
        $regex =~ s/\\\%bugid\\\%/\\d+/;
    }
    $regex = "^$regex";
    return ($self->self_url =~ $regex) ? 1 : 0;
}

##########################
# Vars TIEHASH Interface #
##########################

# Fix the TIEHASH interface (scalar $cgi->Vars) to return and accept 
# arrayrefs.
sub STORE {
    my $self = shift;
    my ($param, $value) = @_;
    if (defined $value and ref $value eq 'ARRAY') {
        return $self->param(-name => $param, -value => $value);
    }
    return $self->SUPER::STORE(@_);
}

sub FETCH {
    my ($self, $param) = @_;
    return $self if $param eq 'CGI'; # CGI.pm did this, so we do too.
    my @result = $self->param($param);
    return undef if !scalar(@result);
    return $result[0] if scalar(@result) == 1;
    return \@result;
}

# For the Vars TIEHASH interface: the normal CGI.pm DELETE doesn't return 
# the value deleted, but Perl's "delete" expects that value.
sub DELETE {
    my ($self, $param) = @_;
    my $value = $self->FETCH($param);
    $self->delete($param);
    return $value;
}

1;

__END__

=head1 NAME

Bugzilla::CGI - CGI handling for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::CGI;

  my $cgi = new Bugzilla::CGI();

=head1 DESCRIPTION

This package inherits from the standard CGI module, to provide additional
Bugzilla-specific functionality. In general, see L<the CGI.pm docs|CGI> for
documention.

=head1 CHANGES FROM L<CGI.PM|CGI>

Bugzilla::CGI has some differences from L<CGI.pm|CGI>.

=over 4

=item C<cgi_error> is automatically checked

After creating the CGI object, C<Bugzilla::CGI> automatically checks
I<cgi_error>, and throws a CodeError if a problem is detected.

=back

=head1 ADDITIONAL FUNCTIONS

I<Bugzilla::CGI> also includes additional functions.

=over 4

=item C<canonicalise_query(@exclude)>

This returns a sorted string of the parameters, suitable for use in a url.
Values in C<@exclude> are not included in the result.

=item C<send_cookie>

This routine is identical to the cookie generation part of CGI.pm's C<cookie>
routine, except that it knows about Bugzilla's cookie_path and cookie_domain
parameters and takes them into account if necessary.
This should be used by all Bugzilla code (instead of C<cookie> or the C<-cookie>
argument to C<header>), so that under mod_perl the headers can be sent
correctly, using C<print> or the mod_perl APIs as appropriate.

To remove (expire) a cookie, use C<remove_cookie>.

=item C<remove_cookie>

This is a wrapper around send_cookie, setting an expiry date in the past,
effectively removing the cookie.

As its only argument, it takes the name of the cookie to expire.

=item C<redirect_to_https>

This routine redirects the client to the https version of the page that
they're looking at, using the C<sslbase> parameter for the redirection.

Generally you should use L<Bugzilla::Util/do_ssl_redirect_if_required>
instead of calling this directly.

=item C<redirect_to_urlbase>

Redirects from the current URL to one prefixed by the urlbase parameter.

=back

=head1 SEE ALSO

L<CGI|CGI>, L<CGI::Cookie|CGI::Cookie>
