# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService::Server::JSONRPC);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Hook;
use Bugzilla::Util qw(correct_urlbase html_quote);
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(taint_data fix_credentials);

# Load resource modules
use Bugzilla::WebService::Server::REST::Resources::Bug;
use Bugzilla::WebService::Server::REST::Resources::Bugzilla;
use Bugzilla::WebService::Server::REST::Resources::Classification;
use Bugzilla::WebService::Server::REST::Resources::Component;
use Bugzilla::WebService::Server::REST::Resources::FlagType;
use Bugzilla::WebService::Server::REST::Resources::Group;
use Bugzilla::WebService::Server::REST::Resources::Product;
use Bugzilla::WebService::Server::REST::Resources::User;
use Bugzilla::WebService::Server::REST::Resources::BugUserLastVisit;

use List::MoreUtils qw(uniq);
use Scalar::Util qw(blessed reftype);
use MIME::Base64 qw(decode_base64);

###########################
# Public Method Overrides #
###########################

sub handle {
    my ($self)  = @_;

    # Determine how the data should be represented. We do this early so
    # errors will also be returned with the proper content type.
    # If no accept header was sent or the content types specified were not
    # matched, we default to the first type in the whitelist.
    $self->content_type($self->_best_content_type(REST_CONTENT_TYPE_WHITELIST()));

    # Using current path information, decide which class/method to
    # use to serve the request. Throw error if no resource was found
    # unless we were looking for OPTIONS
    if (!$self->_find_resource($self->cgi->path_info)) {
        if ($self->request->method eq 'OPTIONS'
            && $self->bz_rest_options)
        {
            my $response = $self->response_header(STATUS_OK, "");
            my $options_string = join(', ', @{ $self->bz_rest_options });
            $response->header('Allow' => $options_string,
                              'Access-Control-Allow-Methods' => $options_string);
            return $self->response($response);
        }

        ThrowUserError("rest_invalid_resource",
                       { path   => $self->cgi->path_info,
                         method => $self->request->method });
    }

    # Dispatch to the proper module
    my $class  = $self->bz_class_name;
    my ($path) = $class =~ /::([^:]+)$/;
    $self->path_info($path);
    delete $self->{dispatch_path};
    $self->dispatch({ $path => $class });

    my $params = $self->_retrieve_json_params;

    fix_credentials($params);

    # Fix includes/excludes for each call
    rest_include_exclude($params);

    # Set callback name if exists
    $self->_bz_callback($params->{'callback'}) if $params->{'callback'};

    Bugzilla->input_params($params);

    # Set the JSON version to 1.1 and the id to the current urlbase
    # also set up the correct handler method
    my $obj = {
        version => '1.1',
        id      => correct_urlbase(),
        method  => $self->bz_method_name,
        params  => $params
    };

    # Execute the handler
    my $result = $self->_handle($obj);

    if (!$self->error_response_header) {
        return $self->response(
            $self->response_header($self->bz_success_code || STATUS_OK, $result));
    }

    $self->response($self->error_response_header);
}

sub response {
    my ($self, $response) = @_;

    # If we have thrown an error, the 'error' key will exist
    # otherwise we use 'result'. JSONRPC returns other data
    # along with the result/error such as version and id which
    # we will strip off for REST calls.
    my $content = $response->content;
    my $json_data = {};
    if ($content) {
        $json_data = $self->json->decode($content);
    }

    my $result = {};
    if (exists $json_data->{error}) {
        $result = $json_data->{error};
        $result->{error} = $self->type('boolean', 1);
        $result->{documentation} = REST_DOC;
        delete $result->{'name'}; # Remove JSONRPCError
    }
    elsif (exists $json_data->{result}) {
        $result = $json_data->{result};
    }

    # The result needs to be a valid JSON data structure
    # and not a undefined or scalar value.
    if (!ref $result
        || blessed($result)
        || (ref $result ne 'HASH' && ref $result ne 'ARRAY'))
    {
        $result = { result => $result };
    }

    Bugzilla::Hook::process('webservice_rest_response',
        { rpc => $self, result => \$result, response => $response });

    # Access Control
    $response->header("Access-Control-Allow-Origin", "*");
    $response->header("Access-Control-Allow-Headers", "origin, content-type, accept, x-requested-with");

    # ETag support
    my $etag = $self->bz_etag;
    $self->bz_etag($result) if !$etag;

    # If accessing through web browser, then display in readable format
    if ($self->content_type eq 'text/html') {
        $result = $self->json->pretty->canonical->allow_nonref->encode($result);

        my $template = Bugzilla->template;
        $content = "";
        $template->process("rest.html.tmpl", { result => $result }, \$content)
            || ThrowTemplateError($template->error());

        $response->content_type('text/html');
    }
    else {
        $content = $self->json->encode($result);
    }

    $response->content($content);

    $self->SUPER::response($response);
}

#######################################
# Bugzilla::WebService Implementation #
#######################################

sub handle_login {
    my $self = shift;

    # If we're being called using GET, we don't allow cookie-based or Env
    # login, because GET requests can be done cross-domain, and we don't
    # want private data showing up on another site unless the user
    # explicitly gives that site their username and password. (This is
    # particularly important for JSONP, which would allow a remote site
    # to use private data without the user's knowledge, unless we had this
    # protection in place.) We do allow this for GET /login as we need to
    # for Bugzilla::Auth::Persist::Cookie to create a login cookie that we
    # can also use for Bugzilla_token support. This is OK as it requires
    # a login and password to be supplied and will fail if they are not
    # valid for the user.
    if (!grep($_ eq $self->request->method, ('POST', 'PUT'))
        && !($self->bz_class_name eq 'Bugzilla::WebService::User'
            && $self->bz_method_name eq 'login'))
    {
        # XXX There's no particularly good way for us to get a parameter
        # to Bugzilla->login at this point, so we pass this information
        # around using request_cache, which is a bit of a hack. The
        # implementation of it is in Bugzilla::Auth::Login::Stack.
        Bugzilla->request_cache->{'auth_no_automatic_login'} = 1;
    }

    my $class = $self->bz_class_name;
    my $method = $self->bz_method_name;
    my $full_method = $class . "." . $method;

    # Bypass JSONRPC::handle_login
    Bugzilla::WebService::Server->handle_login($class, $method, $full_method);
}

############################
# Private Method Overrides #
############################

# We do not want to run Bugzilla::WebService::Server::JSONRPC->_find_prodedure
# as it determines the method name differently.
sub _find_procedure {
    my $self = shift;
    if ($self->isa('JSON::RPC::Server::CGI')) {
        return JSON::RPC::Server::_find_procedure($self, @_);
    }
    else {
        return JSON::RPC::Legacy::Server::_find_procedure($self, @_);
    }
}

sub _argument_type_check {
    my $self = shift;
    my $params;

    if ($self->isa('JSON::RPC::Server::CGI')) {
        $params = JSON::RPC::Server::_argument_type_check($self, @_);
    }
    else {
        $params = JSON::RPC::Legacy::Server::_argument_type_check($self, @_);
    }

    # JSON-RPC 1.0 requires all parameters to be passed as an array, so
    # we just pull out the first item and assume it's an object.
    my $params_is_array;
    if (ref $params eq 'ARRAY') {
        $params = $params->[0];
        $params_is_array = 1;
    }

    taint_data($params);

    # Now, convert dateTime fields on input.
    my $method = $self->bz_method_name;
    my $pkg = $self->{dispatch_path}->{$self->path_info};
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

    # This is the best time to do login checks.
    $self->handle_login();

    # Bugzilla::WebService packages call internal methods like
    # $self->_some_private_method. So we have to inherit from
    # that class as well as this Server class.
    my $new_class = ref($self) . '::' . $pkg;
    my $isa_string = 'our @ISA = qw(' . ref($self) . " $pkg)";
    eval "package $new_class;$isa_string;";
    bless $self, $new_class;

    # Allow extensions to modify the params post login
    Bugzilla::Hook::process('webservice_rest_request',
                            { rpc => $self, params => $params });

    if ($params_is_array) {
        $params = [$params];
    }

    return $params;
}

###################
# Utility Methods #
###################

sub bz_method_name {
    my ($self, $method) = @_;
    $self->{_bz_method_name} = $method if $method;
    return $self->{_bz_method_name};
}

sub bz_class_name {
    my ($self, $class) = @_;
    $self->{_bz_class_name} = $class if $class;
    return $self->{_bz_class_name};
}

sub bz_success_code {
    my ($self, $value) = @_;
    $self->{_bz_success_code} = $value if $value;
    return $self->{_bz_success_code};
}

sub bz_rest_params {
    my ($self, $params) = @_;
    $self->{_bz_rest_params} = $params if $params;
    return $self->{_bz_rest_params};
}

sub bz_rest_options {
    my ($self, $options) = @_;
    $self->{_bz_rest_options} = $options if $options;
    return $self->{_bz_rest_options};
}

sub rest_include_exclude {
    my ($params) = @_;

    if ($params->{'include_fields'} && !ref $params->{'include_fields'}) {
        $params->{'include_fields'} = [ split(/[\s+,]/, $params->{'include_fields'}) ];
    }
    if ($params->{'exclude_fields'} && !ref $params->{'exclude_fields'}) {
        $params->{'exclude_fields'} = [ split(/[\s+,]/, $params->{'exclude_fields'}) ];
    }

    return $params;
}

##########################
# Private Custom Methods #
##########################

sub _retrieve_json_params {
    my $self = shift;

    # Make a copy of the current input_params rather than edit directly
    my $params = {};
    %{$params} = %{ Bugzilla->input_params };

    # First add any parameters we were able to pull out of the path
    # based on the resource regexp and combine with the normal URL
    # parameters.
    if (my $rest_params = $self->bz_rest_params) {
        foreach my $param (keys %$rest_params) {
            # If the param does not already exist or if the
            # rest param is a single value, add it to the
            # global params.
            if (!exists $params->{$param} || !ref $rest_params->{$param}) {
                $params->{$param} = $rest_params->{$param};
            }
            # If rest_param is a list then add any extra values to the list
            elsif (ref $rest_params->{$param}) {
                my @extra_values = ref $params->{$param}
                                   ? @{ $params->{$param} }
                                   : ($params->{$param});
                $params->{$param}
                    = [ uniq (@{ $rest_params->{$param} }, @extra_values) ];
            }
        }
    }

    # Any parameters passed in in the body of a non-GET request will override
    # any parameters pull from the url path. Otherwise non-unique keys are
    # combined.
    if ($self->request->method ne 'GET') {
        my $extra_params = {};
        # We do this manually because CGI.pm doesn't understand JSON strings.
        my $json = delete $params->{'POSTDATA'} || delete $params->{'PUTDATA'};
        if ($json) {
            eval { $extra_params = $self->json->decode($json); };
            if ($@) {
                ThrowUserError('json_rpc_invalid_params', { err_msg  => $@ });
            }
        }

        # Allow parameters in the query string if request was non-GET.
        # Note: parameters in query string body override any matching
        # parameters in the request body.
        foreach my $param ($self->cgi->url_param()) {
            $extra_params->{$param} = $self->cgi->url_param($param);
        }

        %{$params} = (%{$params}, %{$extra_params}) if %{$extra_params};
    }

    return $params;
}

sub _find_resource {
    my ($self, $path) = @_;

    # Load in the WebService module from the dispatch map and then call
    # $module->rest_resources to get the resources array ref.
    my $resources = {};
    foreach my $module (values %{ $self->{dispatch_path} }) {
        eval("require $module") || die $@;
        next if !$module->can('rest_resources');
        $resources->{$module} = $module->rest_resources;
    }

    Bugzilla::Hook::process('webservice_rest_resources',
                            { rpc => $self, resources => $resources });

    # Use the resources hash from each module loaded earlier to determine
    # which handler to use based on a regex match of the CGI path.
    # Also any matches found in the regex will be passed in later to the
    # handler for possible use.
    my $request_method = $self->request->method;

    my (@matches, $handler_found, $handler_method, $handler_class);
    foreach my $class (keys %{ $resources }) {
        # The resource data for each module needs to be
        # an array ref with an even number of elements
        # to work correctly.
        next if (ref $resources->{$class} ne 'ARRAY'
                 || scalar @{ $resources->{$class} } % 2 != 0);

        while (my $regex = shift @{ $resources->{$class} }) {
            my $options_data = shift @{ $resources->{$class} };
            next if ref $options_data ne 'HASH';

            if (@matches = ($path =~ $regex)) {
                # If a specific path is accompanied by a OPTIONS request
                # method, the user is asking for a list of possible request
                # methods for a specific path.
                $self->bz_rest_options([ keys %{ $options_data } ]);

                if ($options_data->{$request_method}) {
                    my $resource_data = $options_data->{$request_method};
                    $self->bz_class_name($class);

                    # The method key/value can be a simple scalar method name
                    # or a anonymous subroutine so we execute it here.
                    my $method = ref $resource_data->{method} eq 'CODE'
                                 ? $resource_data->{method}->($self)
                                 : $resource_data->{method};
                    $self->bz_method_name($method);

                    # Pull out any parameters parsed from the URL path
                    # and store them for use by the method.
                    if ($resource_data->{params}) {
                        $self->bz_rest_params($resource_data->{params}->(@matches));
                    }

                    # If a special success code is needed for this particular
                    # method, then store it for later when generating response.
                    if ($resource_data->{success_code}) {
                        $self->bz_success_code($resource_data->{success_code});
                    }
                    $handler_found = 1;
                }
            }
            last if $handler_found;
        }
        last if $handler_found;
    }

    return $handler_found;
}

sub _best_content_type {
    my ($self, @types) = @_;
    return ($self->_simple_content_negotiation(@types))[0] || '*/*';
}

sub _simple_content_negotiation {
    my ($self, @types) = @_;
    my @accept_types = $self->_get_content_prefs();
    # Return the types as-is if no accept header sent, since sorting will be a no-op.
    if (!@accept_types) {
        return @types;
    }
    my $score = sub { $self->_score_type(shift, @accept_types) };
    return sort {$score->($b) <=> $score->($a)} @types;
}

sub _score_type {
    my ($self, $type, @accept_types) = @_;
    my $score = scalar(@accept_types);
    for my $accept_type (@accept_types) {
        return $score if $type eq $accept_type;
        $score--;
    }
    return 0;
}

sub _get_content_prefs {
    my $self = shift;
    my $default_weight = 1;
    my @prefs;

    # Parse the Accept header, and save type name, score, and position.
    my @accept_types = split /,/, $self->cgi->http('accept') || '';
    my $order = 0;
    for my $accept_type (@accept_types) {
        my ($weight) = ($accept_type =~ /q=(\d\.\d+|\d+)/);
        my ($name) = ($accept_type =~ m#(\S+/[^;]+)#);
        next unless $name;
        push @prefs, { name => $name, order => $order++};
        if (defined $weight) {
            $prefs[-1]->{score} = $weight;
        } else {
            $prefs[-1]->{score} = $default_weight;
            $default_weight -= 0.001;
        }
    }

    # Sort the types by score, subscore by order, and pull out just the name
    @prefs = map {$_->{name}} sort {$b->{score} <=> $a->{score} ||
                                    $a->{order} <=> $b->{order}} @prefs;
    return @prefs;
}

1;

__END__

=head1 NAME

Bugzilla::WebService::Server::REST - The REST Interface to Bugzilla

=head1 DESCRIPTION

This documentation describes things about the Bugzilla WebService that
are specific to REST. For a general overview of the Bugzilla WebServices,
see L<Bugzilla::WebService>. The L<Bugzilla::WebService::Server::REST>
module is a sub-class of L<Bugzilla::WebService::Server::JSONRPC> so any
method documentation not found here can be viewed in it's POD.

Please note that I<everything> about this REST interface is
B<EXPERIMENTAL>. If you want a fully stable API, please use the
C<Bugzilla::WebService::Server::XMLRPC|XML-RPC> interface.

=head1 CONNECTING

The endpoint for the REST interface is the C<rest.cgi> script in
your Bugzilla installation. For example, if your Bugzilla is at
C<bugzilla.yourdomain.com>, to access the API and load a bug,
you would use C<http://bugzilla.yourdomain.com/rest.cgi/bug/35>.

If using Apache and mod_rewrite is installed and enabled, you can
simplify the endpoint by changing /rest.cgi/ to something like /rest/
or something similar. So the same example from above would be:
C<http://bugzilla.yourdomain.com/rest/bug/35> which is simpler to remember.

Add this to your .htaccess file:

  <IfModule mod_rewrite.c>
    RewriteEngine On
    RewriteRule ^rest/(.*)$ rest.cgi/$1 [NE]
  </IfModule>

=head1 BROWSING

If the Accept: header of a request is set to text/html (as it is by an
ordinary web browser) then the API will return the JSON data as a HTML
page which the browser can display. In other words, you can play with the
API using just your browser and see results in a human-readable form.
This is a good way to try out the various GET calls, even if you can't use
it for POST or PUT.

=head1 DATA FORMAT

The REST API only supports JSON input, and either JSON and JSONP output.
So objects sent and received must be in JSON format. Basically since
the REST API is a sub class of the JSONRPC API, you can refer to
L<JSONRPC|Bugzilla::WebService::Server::JSONRPC> for more information
on data types that are valid for REST.

On every request, you must set both the "Accept" and "Content-Type" HTTP
headers to the MIME type of the data format you are using to communicate with
the API. Content-Type tells the API how to interpret your request, and Accept
tells it how you want your data back. "Content-Type" must be "application/json".
"Accept" can be either that, or "application/javascript" for JSONP - add a "callback"
parameter to name your callback.

Parameters may also be passed in as part of the query string  for non-GET requests
and will override any matching parameters in the request body.

=head1 AUTHENTICATION

Along with viewing data as an anonymous user, you may also see private information
if you have a Bugzilla account by providing your login credentials.

=over

=item Login name and password

Pass in as query parameters of any request:

login=fred@example.com&password=ilovecheese

Remember to URL encode any special characters, which are often seen in passwords and to
also enable SSL support.

=item Login token

By calling GET /login?login=fred@example.com&password=ilovecheese, you get back
a C<token> value which can then be passed to each subsequent call as
authentication. This is useful for third party clients that cannot use cookies
and do not want to store a user's login and password in the client. You can also
pass in "token" as a convenience.

=back

=head1 ERRORS

When an error occurs over REST, a hash structure is returned with the key C<error>
set to C<true>.

The error contents look similar to:

 { "error": true, "message": "Some message here", "code": 123 }

Every error has a "code", as described in L<Bugzilla::WebService/ERRORS>.
Errors with a numeric C<code> higher than 100000 are errors thrown by
the JSON-RPC library that Bugzilla uses, not by Bugzilla.

=head1 UTILITY FUNCTIONS

=over

=item B<handle>

This method overrides the handle method provided by JSONRPC so that certain
actions related to REST such as determining the proper resource to use,
loading query parameters, etc. can be done before the proper WebService
method is executed.

=item B<response>

This method overrides the response method provided by JSONRPC so that
the response content can be altered for REST before being returned to
the client.

=item B<handle_login>

This method determines the proper WebService all to make based on class
and method name determined earlier. Then calls L<Bugzilla::WebService::Server::handle_login>
which will attempt to authenticate the client.

=item B<bz_method_name>

The WebService method name that matches the path used by the client.

=item B<bz_class_name>

The WebService class containing the method that matches the path used by the client.

=item B<bz_rest_params>

Each REST resource contains a hash key called C<params> that is a subroutine reference.
This subroutine will return a hash structure based on matched values from the path
information that is formatted properly for the WebService method that will be called.

=item B<bz_rest_options>

When a client uses the OPTIONS request method along with a specific path, they are
requesting the list of request methods that are valid for the path. Such as for the
path /bug, the valid request methods are GET (search) and POST (create). So the
client would receive in the response header, C<Access-Control-Allow-Methods: GET, POST>.

=item B<bz_success_code>

Each resource can specify a specific SUCCESS CODE if the operation completes successfully.
OTherwise STATUS OK (200) is the default returned.

=item B<rest_include_exclude>

Normally the WebService methods required C<include_fields> and C<exclude_fields> to be an
array of field names. REST allows for the values for these to be instead comma delimited
string of field names. This method converts the latter into the former so the WebService
methods will not complain.

=back

=head1 SEE ALSO

L<Bugzilla::WebService>
