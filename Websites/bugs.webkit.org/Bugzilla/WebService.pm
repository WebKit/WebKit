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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::WebService;

use strict;
use Bugzilla::WebService::Constants;
use Bugzilla::Util;
use Date::Parse;

sub fail_unimplemented {
    my $this = shift;

    die SOAP::Fault
        ->faultcode(ERROR_UNIMPLEMENTED)
        ->faultstring('Service Unimplemented');
}

sub datetime_format {
    my ($self, $date_string) = @_;

    my $time = str2time($date_string);
    my ($sec, $min, $hour, $mday, $mon, $year) = localtime $time;
    # This format string was stolen from SOAP::Utils->format_datetime,
    # which doesn't work but which has almost the right format string.
    my $iso_datetime = sprintf('%d%02d%02dT%02d:%02d:%02d',
        $year + 1900, $mon + 1, $mday, $hour, $min, $sec);
    return $iso_datetime;
}

sub handle_login {
    my ($classes, $action, $uri, $method) = @_;

    my $class = $classes->{$uri};
    eval "require $class";

    return if $class->login_exempt($method);
    Bugzilla->login();

    # Even though we check for the need to redirect in
    # Bugzilla->login() we check here again since Bugzilla->login()
    # does not know what the current XMLRPC method is. Therefore
    # ssl_require_redirect in Bugzilla->login() will have returned 
    # false if system was configured to redirect for authenticated 
    # sessions and the user was not yet logged in.
    # So here we pass in the method name to ssl_require_redirect so
    # it can then check for the extra case where the method equals
    # User.login, which we would then need to redirect if not
    # over a secure connection. 
    my $full_method = $uri . "." . $method;
    Bugzilla->cgi->require_https(Bugzilla->params->{'sslbase'})
        if ssl_require_redirect($full_method);

    return;
}

# For some methods, we shouldn't call Bugzilla->login before we call them
use constant LOGIN_EXEMPT => { };

sub login_exempt {
    my ($class, $method) = @_;

    return $class->LOGIN_EXEMPT->{$method};
}

1;

package Bugzilla::WebService::XMLRPC::Transport::HTTP::CGI;
use strict;
eval { require XMLRPC::Transport::HTTP; };
our @ISA = qw(XMLRPC::Transport::HTTP::CGI);

sub initialize {
    my $self = shift;
    my %retval = $self->SUPER::initialize(@_);
    $retval{'serializer'} = Bugzilla::WebService::XMLRPC::Serializer->new;
    return %retval;
}

sub make_response {
    my $self = shift;

    $self->SUPER::make_response(@_);

    # XMLRPC::Transport::HTTP::CGI doesn't know about Bugzilla carrying around
    # its cookies in Bugzilla::CGI, so we need to copy them over.
    foreach (@{Bugzilla->cgi->{'Bugzilla_cookie_list'}}) {
        $self->response->headers->push_header('Set-Cookie', $_);
    }
}

1;

# This package exists to fix a UTF-8 bug in SOAP::Lite.
# See http://rt.cpan.org/Public/Bug/Display.html?id=32952.
package Bugzilla::WebService::XMLRPC::Serializer;
use strict;
# We can't use "use base" because XMLRPC::Serializer doesn't return
# a true value.
eval { require XMLRPC::Lite; };
our @ISA = qw(XMLRPC::Serializer);

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    # This fixes UTF-8.
    $self->{'_typelookup'}->{'base64'} =
        [10, sub { !utf8::is_utf8($_[0]) && $_[0] =~ /[^\x09\x0a\x0d\x20-\x7f]/},
        'as_base64'];
    # This makes arrays work right even though we're a subclass.
    # (See http://rt.cpan.org//Ticket/Display.html?id=34514)
    $self->{'_encodingStyle'} = '';
    return $self;
}

sub as_string {
    my $self = shift;
    my ($value) = @_;
    # Something weird happens with XML::Parser when we have upper-ASCII 
    # characters encoded as UTF-8, and this fixes it.
    utf8::encode($value) if utf8::is_utf8($value) 
                            && $value =~ /^[\x00-\xff]+$/;
    return $self->SUPER::as_string($value);
}

1;

__END__

=head1 NAME

Bugzilla::WebService - The Web Service interface to Bugzilla

=head1 DESCRIPTION

This is the standard API for external programs that want to interact
with Bugzilla. It provides various methods in various modules.

Currently the only method of accessing the API is via XML-RPC. The XML-RPC
standard is described here: L<http://www.xmlrpc.com/spec>

The endpoint for Bugzilla WebServices is the C<xmlrpc.cgi> script in
your Bugzilla installation. For example, if your Bugzilla is at
C<bugzilla.yourdomain.com>, then your XML-RPC client would access the
API via: C<http://bugzilla.yourdomain.com/xmlrpc.cgi>

=head1 CALLING METHODS

Methods are called in the normal XML-RPC fashion. Bugzilla does not currently
implement any extensions to the standard method of XML-RPC method calling.

Methods are grouped into "packages", like C<Bug> for 
L<Bugzilla::WebService::Bug>. So, for example,
L<Bugzilla::WebService::Bug/get>, is called as C<Bug.get> in XML-RPC.

=head1 PARAMETERS

In addition to the standard parameter types like C<int>, C<string>, etc.,
XML-RPC has two data structures, a C<< <struct> >> and an C<< <array> >>.

=head2 Structs

In Perl, we call a C<< <struct> >> a "hash" or a "hashref". You may see
us refer to it that way in the API documentation.

In example code, you will see the characters C<{> and C<}> used to represent
the beginning and end of structs.

For example, here's a struct in XML-RPC:

 <struct>
   <member>
     <name>fruit</name>
     <value><string>oranges</string></value>
   </member>
   <member>
     <name>vegetable</name>
     <value><string>lettuce</string></value>
   </member>
 </struct>

In our example code in these API docs, that would look like:

 { fruit => 'oranges', vegetable => 'lettuce' }

=head2 Arrays

In example code, you will see the characters C<[> and C<]> used to
represent the beginning and end of arrays.

For example, here's an array in XML-RPC:

 <array>
   <data>
     <value><i4>1</i4></value>
     <value><i4>2</i4></value>
     <value><i4>3</i4></value>
   </data>
 </array>

In our example code in these API docs, that would look like:

 [1, 2, 3]

=head2 How Bugzilla WebService Methods Take Parameters

B<All> Bugzilla WebServices functions take their parameters in
a C<< <struct> >>. Another way of saying this would be: All functions
take a single argument, a C<< <struct> >> that contains all parameters.
The names of the parameters listed in the API docs for each function are
the C<name> element for the struct C<member>s.

=head1 LOGGING IN

You can use L<Bugzilla::WebService::User/login> to log in as a Bugzilla 
user. This issues standard HTTP cookies that you must then use in future
calls, so your XML-RPC client must be capable of receiving and transmitting
cookies.

=head1 STABLE, EXPERIMENTAL, and UNSTABLE

Methods are marked B<STABLE> if you can expect their parameters and
return values not to change between versions of Bugzilla. You are 
best off always using methods marked B<STABLE>. We may add parameters
and additional items to the return values, but your old code will
always continue to work with any new changes we make. If we ever break
a B<STABLE> interface, we'll post a big notice in the Release Notes,
and it will only happen during a major new release.

Methods (or parts of methods) are marked B<EXPERIMENTAL> if 
we I<believe> they will be stable, but there's a slight chance that 
small parts will change in the future.

Certain parts of a method's description may be marked as B<UNSTABLE>,
in which case those parts are not guaranteed to stay the same between
Bugzilla versions.

=head1 ERRORS

If a particular webservice call fails, it will throw a standard XML-RPC
error. There will be a numeric error code, and then the description
field will contain descriptive text of the error. Each error that Bugzilla
can throw has a specific code that will not change between versions of
Bugzilla.

The various errors that functions can throw are specified by the
documentation of those functions.

If your code needs to know what error Bugzilla threw, use the numeric
code. Don't try to parse the description, because that may change
from version to version of Bugzilla.

Note that if you display the error to the user in an HTML program, make
sure that you properly escape the error, as it will not be HTML-escaped.

=head2 Transient vs. Fatal Errors

If the error code is a number greater than 0, the error is considered
"transient," which means that it was an error made by the user, not
some problem with Bugzilla itself.

If the error code is a number less than 0, the error is "fatal," which
means that it's some error in Bugzilla itself that probably requires
administrative attention.

Negative numbers and positive numbers don't overlap. That is, if there's
an error 302, there won't be an error -302.

=head2 Unknown Errors

Sometimes a function will throw an error that doesn't have a specific
error code. In this case, the code will be C<-32000> if it's a "fatal"
error, and C<32000> if it's a "transient" error.
