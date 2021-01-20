# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This is the base class for $self in WebService method calls. For the 
# actual RPC server, see Bugzilla::WebService::Server and its subclasses.
package Bugzilla::WebService;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Server;

# Used by the JSON-RPC server to convert incoming date fields apprpriately.
use constant DATE_FIELDS => {};
# Used by the JSON-RPC server to convert incoming base64 fields appropriately.
use constant BASE64_FIELDS => {};

# For some methods, we shouldn't call Bugzilla->login before we call them
use constant LOGIN_EXEMPT => { };

# Used to allow methods to be called in the JSON-RPC WebService via GET.
# Methods that can modify data MUST not be listed here.
use constant READ_ONLY => ();

# Whitelist of methods that a client is allowed to access when making
# an API call.
use constant PUBLIC_METHODS => ();

sub login_exempt {
    my ($class, $method) = @_;
    return $class->LOGIN_EXEMPT->{$method};
}

1;

__END__

=head1 NAME

Bugzilla::WebService - The Web Service interface to Bugzilla

=head1 DESCRIPTION

This is the standard API for external programs that want to interact
with Bugzilla. It provides various methods in various modules.

You can interact with this API via
L<XML-RPC|Bugzilla::WebService::Server::XMLRPC>,
L<JSON-RPC|Bugzilla::WebService::Server::JSONRPC> or
L<REST|Bugzilla::WebService::Server::REST>.

=head1 CALLING METHODS

Methods are grouped into "packages", like C<Bug> for
L<Bugzilla::WebService::Bug>. So, for example,
L<Bugzilla::WebService::Bug/get>, is called as C<Bug.get>.

For REST, the "package" is more determined by the path
used to access the resource. See each relevant method
for specific details on how to access via REST.

=head1 PARAMETERS

The Bugzilla API takes the following various types of parameters:

=over

=item C<int>

Integer. May be null.

=item C<double>

A floating-point number. May be null.

=item C<string>

A string. May be null.

=item C<email>

A string representing an email address. This value, when returned, 
may be filtered based on if the user is logged in or not. May be null.

=item C<dateTime>

A date/time. Represented differently in different interfaces to this API.
May be null.

=item C<boolean>

True or false.

=item C<base64>

A base64-encoded string. This is the only way to transfer
binary data via the WebService.

=item C<array>

An array. There may be mixed types in an array.

In example code, you will see the characters C<[> and C<]> used to
represent the beginning and end of arrays.

In our example code in these API docs, an array that contains the numbers
1, 2, and 3 would look like:

 [1, 2, 3]

=item C<struct>

A mapping of keys to values. Called a "hash", "dict", or "map" in some
other programming languages. We sometimes call this a "hash" in the API
documentation.

The keys are strings, and the values can be any type.

In example code, you will see the characters C<{> and C<}> used to represent
the beginning and end of structs.

For example, a struct with an "fruit" key whose value is "oranges",
and a "vegetable" key whose value is "lettuce" would look like:

 { fruit => 'oranges', vegetable => 'lettuce' }

=back

=head2 How Bugzilla WebService Methods Take Parameters

B<All> Bugzilla WebService functions use I<named> parameters.
The individual C<Bugzilla::WebService::Server> modules explain
how this is implemented for those frontends.

=head1 LOGGING IN

Some methods do not require you to log in. An example of this is Bug.get.
However, authenticating yourself allows you to see non public information. For
example, a bug that is not publicly visible.

There are two ways to authenticate yourself:

=over

=item C<Bugzilla_api_key>

B<Added in Bugzilla 5.0>

You can specify C<Bugzilla_api_key> as an argument to any WebService method, and
you will be logged in as that user if the key is correct, and has not been
revoked. You can set up an API key by using the 'API Key' tab in the
Preferences pages.

=item C<Bugzilla_login> and C<Bugzilla_password>

B<Added in Bugzilla 3.6>

You can specify C<Bugzilla_login> and C<Bugzilla_password> as arguments
to any WebService method, and you will be logged in as that user if your
credentials are correct. Here are the arguments you can specify to any
WebService method to perform a login:

=over

=item C<Bugzilla_login> (string) - A user's login name.

=item C<Bugzilla_password> (string) - That user's password.

=item C<Bugzilla_restrictlogin> (boolean) - Optional. If true,
then your login will only be valid for your IP address.

=back

The C<Bugzilla_restrictlogin> option is only used when you have also
specified C<Bugzilla_login> and C<Bugzilla_password>. This value will be
deprecated in the release after Bugzilla 5.0 and you will be required to
pass the Bugzilla_login and Bugzilla_password for every call.

For REST, you may also use the C<login> and C<password> variable
names instead of C<Bugzilla_login> and C<Bugzilla_password> as a
convenience. You may also use C<token> instead of C<Bugzilla_token>.

=back

There are also two deprecreated methods of authentications. This will be
removed in the version after Bugzilla 5.0.

=over

=item C<User.login>

You can use L<Bugzilla::WebService::User/login> to log in as a Bugzilla
user. This issues a token that you must then use in future calls.

=item C<Bugzilla_token>

B<Added in Bugzilla 4.4.3>

You can specify C<Bugzilla_token> as argument to any WebService method,
and you will be logged in as that user if the token is correct. This is
the token returned when calling C<User.login> mentioned above.

An error is thrown if you pass an invalid token and you will need to log
in again to get a new token.

Token support was added in Bugzilla B<5.0> and support for login cookies
has been dropped for security reasons.

=back

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

If a particular webservice call fails, it will throw an error in the
appropriate format for the frontend that you are using. For all frontends,
there is at least a numeric error code and descriptive text for the error.

The various errors that functions can throw are specified by the
documentation of those functions.

Each error that Bugzilla can throw has a specific numeric code that will
not change between versions of Bugzilla. If your code needs to know what
error Bugzilla threw, use the numeric code. Don't try to parse the
description, because that may change from version to version of Bugzilla.

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

=head1 COMMON PARAMETERS

Many Webservice methods take similar arguments. Instead of re-writing
the documentation for each method, we document the parameters here, once,
and then refer back to this documentation from the individual methods
where these parameters are used.

=head2 Limiting What Fields Are Returned

Many WebService methods return an array of structs with various
fields in the structs. (For example, L<Bugzilla::WebService::Bug/get>
returns a list of C<bugs> that have fields like C<id>, C<summary>, 
C<creation_time>, etc.)

These parameters allow you to limit what fields are present in
the structs, to possibly improve performance or save some bandwidth.

=over

=item C<include_fields> 

C<array> An array of strings, representing the (case-sensitive) names of
fields in the return value. Only the fields specified in this hash will
be returned, the rest will not be included.

If you specify an empty array, then this function will return empty
hashes.

Invalid field names are ignored.

Example:

  User.get( ids => [1], include_fields => ['id', 'name'] )

would return something like:

  { users => [{ id => 1, name => 'user@domain.com' }] }

Note for REST, C<include_fields> may instead be a comma delimited string
for GET type requests.

=item C<exclude_fields>

C<array> An array of strings, representing the (case-sensitive) names of
fields in the return value. The fields specified will not be included in
the returned hashes.

If you specify all the fields, then this function will return empty
hashes.

Some RPC calls support specifying sub fields. If an RPC call states that
it support sub field restrictions, you can restrict what information is
returned within the first field. For example, if you call Product.get
with an include_fields of components.name, then only the component name
would be returned (and nothing else). You can include the main field,
and exclude a sub field.

Invalid field names are ignored.

Specifying fields here overrides C<include_fields>, so if you specify a
field in both, it will be excluded, not included.

Example:

  User.get( ids => [1], exclude_fields => ['name'] )

would return something like:

  { users => [{ id => 1, real_name => 'John Smith' }] }

Note for REST, C<exclude_fields> may instead be a comma delimited string
for GET type requests.

=back

There are several shortcut identifiers to ask for only certain groups of
fields to be returned or excluded.

=over

=item C<_all>

All possible fields are returned if C<_all> is specified in C<include_fields>.

=item C<_default>

These fields are returned if C<include_fields> is empty or C<_default>
is specified. All fields described in the documentation are returned
by default unless specified otherwise.

=item C<_extra>

These fields are not returned by default and need to be manually specified
in C<include_fields> either by field name, or using C<_extra>.

=item C<_custom>

Only custom fields are returned if C<_custom> is specified in C<include_fields>.
This is normally specific to bug objects and not relevant for other returned
objects.

=back

=head1 SEE ALSO

=head2 Server Types

=over

=item L<Bugzilla::WebService::Server::XMLRPC>

=item L<Bugzilla::WebService::Server::JSONRPC>

=back

=head2 WebService Modules

=over

=item L<Bugzilla::WebService::Bug>

=item L<Bugzilla::WebService::Bugzilla>

=item L<Bugzilla::WebService::Classification>

=item L<Bugzilla::WebService::FlagType>

=item L<Bugzilla::WebService::Component>

=item L<Bugzilla::WebService::Group>

=item L<Bugzilla::WebService::Product>

=item L<Bugzilla::WebService::User>

=back

=head1 B<Methods in need of POD>

=over

=item login_exempt

=back
