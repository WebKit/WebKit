# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::JSONRPC;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Server;
BEGIN {
    our @ISA = qw(Bugzilla::WebService::Server);

    if (eval { require JSON::RPC::Server::CGI }) {
        unshift(@ISA, 'JSON::RPC::Server::CGI');
    }
    else {
        require JSON::RPC::Legacy::Server::CGI;
        unshift(@ISA, 'JSON::RPC::Legacy::Server::CGI');
    }
}

use Bugzilla::Error;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(taint_data fix_credentials);
use Bugzilla::Util;

use HTTP::Message;
use MIME::Base64 qw(decode_base64 encode_base64);
use List::MoreUtils qw(none);

#####################################
# Public JSON::RPC Method Overrides #
#####################################

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    Bugzilla->_json_server($self);
    $self->dispatch(WS_DISPATCH);
    $self->return_die_message(1);
    return $self;
}

sub create_json_coder {
    my $self = shift;
    my $json = $self->SUPER::create_json_coder(@_);
    $json->allow_blessed(1);
    $json->convert_blessed(1);
    # This may seem a little backwards, but what this really means is
    # "don't convert our utf8 into byte strings, just leave it as a
    # utf8 string."
    $json->utf8(0) if Bugzilla->params->{'utf8'};
    return $json;
}

# Override the JSON::RPC method to return our CGI object instead of theirs.
sub cgi { return Bugzilla->cgi; }

sub response_header {
    my $self = shift;
    # The HTTP body needs to be bytes (not a utf8 string) for recent
    # versions of HTTP::Message, but JSON::RPC::Server doesn't handle this
    # properly. $_[1] is the HTTP body content we're going to be sending.
    if (utf8::is_utf8($_[1])) {
        utf8::encode($_[1]);
        # Since we're going to just be sending raw bytes, we need to
        # set STDOUT to not expect utf8.
        disable_utf8();
    }
    return $self->SUPER::response_header(@_);
}

sub response {
    my ($self, $response) = @_;
    my $cgi = $self->cgi;

    # Implement JSONP.
    if (my $callback = $self->_bz_callback) {
        my $content = $response->content;
        # Prepend the JSONP response with /**/ in order to protect
        # against possible encoding attacks (e.g., affecting Flash).
        $response->content("/**/$callback($content)");
    }

    # Use $cgi->header properly instead of just printing text directly.
    # This fixes various problems, including sending Bugzilla's cookies
    # properly.
    my $headers = $response->headers;
    my @header_args;
    foreach my $name ($headers->header_field_names) {
        my @values = $headers->header($name);
        $name =~ s/-/_/g;
        foreach my $value (@values) {
            push(@header_args, "-$name", $value);
        }
    }

    # ETag support
    my $etag = $self->bz_etag;
    if ($etag && $cgi->check_etag($etag)) {
        push(@header_args, "-ETag", $etag);
        print $cgi->header(-status => '304 Not Modified', @header_args);
    }
    else {
        push(@header_args, "-ETag", $etag) if $etag;
        print $cgi->header(-status => $response->code, @header_args);
        print $response->content;
    }
}

# The JSON-RPC 1.1 GET specification is not so great--you can't specify
# data structures as parameters. However, the JSON-RPC 2.0 "JSON-RPC over
# HTTP" spec is excellent, so we are using that for GET requests, instead.
# Spec: http://groups.google.com/group/json-rpc/web/json-rpc-over-http
#
# The one exception is that we don't require the "params" argument to be
# Base64 encoded, because that is ridiculous and obnoxious for JavaScript
# clients.
sub retrieve_json_from_get {
    my $self = shift;
    my $cgi = $self->cgi;

    my %input;

    # Both version and id must be set before any errors are thrown.
    if ($cgi->param('version')) {
        $self->version(scalar $cgi->param('version'));
        $input{version} = $cgi->param('version');
    }
    else {
        $self->version('1.0');
    }

    # The JSON-RPC 2.0 spec says that any request that omits an id doesn't
    # want a response. However, in an HTTP GET situation, it's stupid to
    # expect all clients to specify some id parameter just to get a response,
    # so we don't require it.
    my $id;
    if (defined $cgi->param('id')) {
        $id = $cgi->param('id');
    }
    # However, JSON::RPC does require that an id exist in most cases, in
    # order to throw proper errors. We use the installation's urlbase as
    # the id, in this case.
    else {
        $id = correct_urlbase();
    }
    # Setting _bz_request_id here is required in case we throw errors early,
    # before _handle.
    $self->{_bz_request_id} = $input{id} = $id;

    # _bz_callback can throw an error, so we have to set it here, after we're
    # ready to throw errors.
    $self->_bz_callback(scalar $cgi->param('callback'));

    if (!$cgi->param('method')) {
        ThrowUserError('json_rpc_get_method_required');
    }
    $input{method} = $cgi->param('method');

    my $params;
    if (defined $cgi->param('params')) {
        local $@;
        $params = eval { 
            $self->json->decode(scalar $cgi->param('params')) 
        };
        if ($@) {
            ThrowUserError('json_rpc_invalid_params',
                           { params => scalar $cgi->param('params'),
                             err_msg  => $@ });
        }
    }
    elsif (!$self->version or $self->version ne '1.1') {
        $params = [];
    }
    else {
        $params = {};
    }

    $input{params} = $params;

    my $json = $self->json->encode(\%input);
    return $json;
}

#######################################
# Bugzilla::WebService Implementation #
#######################################

sub type {
    my ($self, $type, $value) = @_;
    
    # This is the only type that does something special with undef.
    if ($type eq 'boolean') {
        return $value ? JSON::true : JSON::false;
    }
    
    return JSON::null if !defined $value;

    my $retval = $value;

    if ($type eq 'int') {
        $retval = int($value);
    }
    if ($type eq 'double') {
        $retval = 0.0 + $value;
    }
    elsif ($type eq 'string') {
        # Forces string context, so that JSON will make it a string.
        $retval = "$value";
    }
    elsif ($type eq 'dateTime') {
        # ISO-8601 "YYYYMMDDTHH:MM:SS" with a literal T
        $retval = $self->datetime_format_outbound($value);
    }
    elsif ($type eq 'base64') {
        utf8::encode($value) if utf8::is_utf8($value);
        $retval = encode_base64($value, '');
    }
    elsif ($type eq 'email' && Bugzilla->params->{'webservice_email_filter'}) {
        $retval = email_filter($value);
    }

    return $retval;
}

sub datetime_format_outbound {
    my $self = shift;
    # YUI expects ISO8601 in UTC time; including TZ specifier
    return $self->SUPER::datetime_format_outbound(@_) . 'Z';
}

sub handle_login {
    my $self = shift;

    # If we're being called using GET, we don't allow cookie-based or Env
    # login, because GET requests can be done cross-domain, and we don't
    # want private data showing up on another site unless the user
    # explicitly gives that site their username and password. (This is
    # particularly important for JSONP, which would allow a remote site
    # to use private data without the user's knowledge, unless we had this
    # protection in place.)
    if ($self->request->method ne 'POST') {
        # XXX There's no particularly good way for us to get a parameter
        # to Bugzilla->login at this point, so we pass this information
        # around using request_cache, which is a bit of a hack. The
        # implementation of it is in Bugzilla::Auth::Login::Stack.
        Bugzilla->request_cache->{auth_no_automatic_login} = 1;
    }

    my $path = $self->path_info;
    my $class = $self->{dispatch_path}->{$path};
    my $full_method = $self->_bz_method_name;
    $full_method =~ /^\S+\.(\S+)/;
    my $method = $1;
    $self->SUPER::handle_login($class, $method, $full_method);
}

######################################
# Private JSON::RPC Method Overrides #
######################################

# Store the ID of the current call, because Bugzilla::Error will need it.
sub _handle {
    my $self = shift;
    my ($obj) = @_;
    $self->{_bz_request_id} = $obj->{id};

    my $result = $self->SUPER::_handle(@_);

    # Set the ETag if not already set in the webservice methods.
    my $etag = $self->bz_etag;
    if (!$etag && ref $result) {
        my $data = $self->json->decode($result)->{'result'};
        $self->bz_etag($data);
    }

    return $result;
}

# Make all error messages returned by JSON::RPC go into the 100000
# range, and bring down all our errors into the normal range.
sub _error {
    my ($self, $id, $code) = (shift, shift, shift);
    # All JSON::RPC errors are less than 1000.
    if ($code < 1000) {
        $code += 100000;
    }
    # Bugzilla::Error adds 100,000 to all *our* errors, so
    # we know they came from us.
    elsif ($code > 100000) {
        $code -= 100000;
    }

    # We can't just set $_[1] because it's not always settable,
    # in JSON::RPC::Server.
    unshift(@_, $id, $code);
    my $json = $self->SUPER::_error(@_);

    # We want to always send the JSON-RPC 1.1 error format, although
    # If we're not in JSON-RPC 1.1, we don't need the silly "name" parameter.
    if (!$self->version or $self->version ne '1.1') {
        my $object = $self->json->decode($json);
        my $message = $object->{error};
        # Just assure that future versions of JSON::RPC don't change the
        # JSON-RPC 1.0 error format.
        if (!ref $message) {
            $object->{error} = {
                code    => $code,
                message => $message,
            };
            $json = $self->json->encode($object);
        }
    }
    return $json;
}

# This handles dispatching our calls to the appropriate class based on
# the name of the method.
sub _find_procedure {
    my $self = shift;

    my $method = shift;
    $self->{_bz_method_name} = $method;

    # This tricks SUPER::_find_procedure into finding the right class.
    $method =~ /^(\S+)\.(\S+)$/;
    $self->path_info($1);
    unshift(@_, $2);

    return $self->SUPER::_find_procedure(@_);
}

# This is a hacky way to do something right before methods are called.
# This is the last thing that JSON::RPC::Server::_handle calls right before
# the method is actually called.
sub _argument_type_check {
    my $self = shift;
    my $params = $self->SUPER::_argument_type_check(@_);

    # JSON-RPC 1.0 requires all parameters to be passed as an array, so
    # we just pull out the first item and assume it's an object.
    my $params_is_array;
    if (ref $params eq 'ARRAY') {
        $params = $params->[0];
        $params_is_array = 1;
    }

    taint_data($params);

    # Now, convert dateTime fields on input.
    $self->_bz_method_name =~ /^(\S+)\.(\S+)$/;
    my ($class, $method) = ($1, $2);
    my $pkg = $self->{dispatch_path}->{$class};
    my @date_fields = @{ $pkg->DATE_FIELDS->{$method} || [] };
    foreach my $field (@date_fields) {
        if (defined $params->{$field}) {
            my $value = $params->{$field};
            if (ref $value eq 'ARRAY') {
                $params->{$field} = 
                    [ map { $self->datetime_format_inbound($_) } @$value ];
            }
            else {
                $params->{$field} = $self->datetime_format_inbound($value);
            }
        }
    }
    my @base64_fields = @{ $pkg->BASE64_FIELDS->{$method} || [] };
    foreach my $field (@base64_fields) {
        if (defined $params->{$field}) {
            $params->{$field} = decode_base64($params->{$field});
        }
    }

    # Update the params to allow for several convenience key/values
    # use for authentication
    fix_credentials($params);

    Bugzilla->input_params($params);

    if ($self->request->method eq 'POST') {
        # CSRF is possible via XMLHttpRequest when the Content-Type header
        # is not application/json (for example: text/plain or
        # application/x-www-form-urlencoded).
        # application/json is the single official MIME type, per RFC 4627.
        my $content_type = $self->cgi->content_type;
        # The charset can be appended to the content type, so we use a regexp.
        if ($content_type !~ m{^application/json(-rpc)?(;.*)?$}i) {
            ThrowUserError('json_rpc_illegal_content_type',
                            { content_type => $content_type });
        }
    }
    else {
        # When being called using GET, we don't allow calling
        # methods that can change data. This protects us against cross-site
        # request forgeries.
        if (!grep($_ eq $method, $pkg->READ_ONLY)) {
            ThrowUserError('json_rpc_post_only', 
                           { method => $self->_bz_method_name });
        }
    }

    # Only allowed methods to be used from our whitelist
    if (none { $_ eq $method} $pkg->PUBLIC_METHODS) {
        ThrowCodeError('unknown_method', { method => $self->_bz_method_name });
    }

    # This is the best time to do login checks.
    $self->handle_login();

    # Bugzilla::WebService packages call internal methods like
    # $self->_some_private_method. So we have to inherit from 
    # that class as well as this Server class.
    my $new_class = ref($self) . '::' . $pkg;
    my $isa_string = 'our @ISA = qw(' . ref($self) . " $pkg)";
    eval "package $new_class;$isa_string;";
    bless $self, $new_class;

    if ($params_is_array) {
        $params = [$params];
    }

    return $params;
}

##########################
# Private Custom Methods #
##########################

# _bz_method_name is stored by _find_procedure for later use.
sub _bz_method_name {
    return $_[0]->{_bz_method_name}; 
}

sub _bz_callback {
    my ($self, $value) = @_;
    if (defined $value) {
        $value = trim($value);
        # We don't use \w because we don't want to allow Unicode here.
        if ($value !~ /^[A-Za-z0-9_\.\[\]]+$/) {
            ThrowUserError('json_rpc_invalid_callback', { callback => $value });
        }
        $self->{_bz_callback} = $value;
        # JSONP needs to be parsed by a JS parser, not by a JSON parser.
        $self->content_type('text/javascript');
    }
    return $self->{_bz_callback};
}

1;

__END__

=head1 NAME

Bugzilla::WebService::Server::JSONRPC - The JSON-RPC Interface to Bugzilla

=head1 DESCRIPTION

This documentation describes things about the Bugzilla WebService that
are specific to JSON-RPC. For a general overview of the Bugzilla WebServices,
see L<Bugzilla::WebService>.

Please note that I<everything> about this JSON-RPC interface is
B<EXPERIMENTAL>. If you want a fully stable API, please use the
C<Bugzilla::WebService::Server::XMLRPC|XML-RPC> interface.

=head1 JSON-RPC

Bugzilla supports both JSON-RPC 1.0 and 1.1. We recommend that you use
JSON-RPC 1.0 instead of 1.1, though, because 1.1 is deprecated.

At some point in the future, Bugzilla may also support JSON-RPC 2.0.

The JSON-RPC standards are described at L<http://json-rpc.org/>.

=head1 CONNECTING

The endpoint for the JSON-RPC interface is the C<jsonrpc.cgi> script in
your Bugzilla installation. For example, if your Bugzilla is at
C<bugzilla.yourdomain.com>, then your JSON-RPC client would access the
API via: C<http://bugzilla.yourdomain.com/jsonrpc.cgi>

=head2 Connecting via GET

The most powerful way to access the JSON-RPC interface is by HTTP POST.
However, for convenience, you can also access certain methods by using GET
(a normal webpage load). Methods that modify the database or cause some
action to happen in Bugzilla cannot be called over GET. Only methods that
simply return data can be used over GET.

For security reasons, when you connect over GET, cookie authentication
is not accepted. If you want to authenticate using GET, you have to
use the C<Bugzilla_login> and C<Bugzilla_password> method described at
L<Bugzilla::WebService/LOGGING IN>.

To connect over GET, simply send the values that you'd normally send for
each JSON-RPC argument as URL parameters, with the C<params> item being
a JSON string. 

The simplest example is a call to C<Bugzilla.time>:

 jsonrpc.cgi?method=Bugzilla.time

Here's a call to C<User.get>, with several parameters:

 jsonrpc.cgi?method=User.get&params=[ { "ids": [1,2], "names": ["user@domain.com"] } ]

Although in reality you would url-encode the C<params> argument, so it would
look more like this:

 jsonrpc.cgi?method=User.get&params=%5B+%7B+%22ids%22%3A+%5B1%2C2%5D%2C+%22names%22%3A+%5B%22user%40domain.com%22%5D+%7D+%5D

You can also specify C<version> as a URL parameter, if you want to specify
what version of the JSON-RPC protocol you're using, and C<id> as a URL
parameter if you want there to be a specific C<id> value in the returned
JSON-RPC response.

=head2 JSONP

When calling the JSON-RPC WebService over GET, you can use the "JSONP"
method of doing cross-domain requests, if you want to access the WebService
directly on a web page from another site. JSONP is described at
L<http://bob.pythonmac.org/archives/2005/12/05/remote-json-jsonp/>.

To use JSONP with Bugzilla's JSON-RPC WebService, simply specify a
C<callback> parameter to jsonrpc.cgi when using it via GET as described above.
For example, here's some HTML you could use to get the data from 
C<Bugzilla.time> on a remote website, using JSONP:

 <script type="text/javascript" 
         src="http://bugzilla.example.com/jsonrpc.cgi?method=Bugzilla.time&amp;callback=foo">

That would call the C<Bugzilla.time> method and pass its value to a function
called C<foo> as the only argument. All the other URL parameters (such as
C<params>, for passing in arguments to methods) that can be passed to
C<jsonrpc.cgi> during GET requests are also available, of course. The above
is just the simplest possible example.

The values returned when using JSONP are identical to the values returned
when not using JSONP, so you will also get error messages if there is an
error.

The C<callback> URL parameter may only contain letters, numbers, periods, and
the underscore (C<_>) character. Including any other characters will cause
Bugzilla to throw an error. (This error will be a normal JSON-RPC response,
not JSONP.)

=head1 PARAMETERS

For JSON-RPC 1.0, the very first parameter should be an object containing
the named parameters. For example, if you were passing two named parameters,
one called C<foo> and the other called C<bar>, the C<params> element of
your JSON-RPC call would look like:

 "params": [{ "foo": 1, "bar": "something" }]

For JSON-RPC 1.1, you can pass parameters either in the above fashion
or using the standard named-parameters mechanism of JSON-RPC 1.1.

C<dateTime> fields are strings in the standard ISO-8601 format:
C<YYYY-MM-DDTHH:MM:SSZ>, where C<T> and C<Z> are a literal T and Z,
respectively. The "Z" means that all times are in UTC timezone--times are
always returned in UTC, and should be passed in as UTC. (Note: The JSON-RPC
interface currently also accepts non-UTC times for any values passed in, if
they include a time-zone specifier that follows the ISO-8601 standard, instead
of "Z" at the end. This behavior is expected to continue into the future, but
to be fully safe for forward-compatibility with all future versions of
Bugzilla, it is safest to pass in all times as UTC with the "Z" timezone
specifier.)

C<base64> fields are strings that have been base64 encoded. Note that
although normal base64 encoding includes newlines to break up the data,
newlines within a string are not valid JSON, so you should not insert
newlines into your base64-encoded string.

All other types are standard JSON types.

=head1 ERRORS

JSON-RPC 1.0 and JSON-RPC 1.1 both return an C<error> element when they
throw an error. In Bugzilla, the error contents look like:

 { message: 'Some message here', code: 123 }

So, for example, in JSON-RPC 1.0, an error response would look like:

 { 
   result: null, 
   error: { message: 'Some message here', code: 123 }, 
   id: 1
 }

Every error has a "code", as described in L<Bugzilla::WebService/ERRORS>.
Errors with a numeric C<code> higher than 100000 are errors thrown by
the JSON-RPC library that Bugzilla uses, not by Bugzilla.

=head1 SEE ALSO

L<Bugzilla::WebService>

=head1 B<Methods in need of POD>

=over

=item response

=item response_header

=item cgi

=item retrieve_json_from_get

=item create_json_coder

=item type

=item handle_login

=item datetime_format_outbound

=back
