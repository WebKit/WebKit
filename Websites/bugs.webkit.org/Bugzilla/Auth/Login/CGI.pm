# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Login::CGI;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Login);
use constant user_can_create_account => 1;

use Bugzilla::Constants;
use Bugzilla::WebService::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Token;

sub get_login_info {
    my ($self) = @_;
    my $params = Bugzilla->input_params;
    my $cgi = Bugzilla->cgi;

    my $login = trim(delete $params->{'Bugzilla_login'});
    my $password = delete $params->{'Bugzilla_password'};
    # The token must match the cookie to authenticate the request.
    my $login_token = delete $params->{'Bugzilla_login_token'};
    my $login_cookie = $cgi->cookie('Bugzilla_login_request_cookie');

    my $valid = 0;
    # If the web browser accepts cookies, use them.
    if ($login_token && $login_cookie) {
        my ($time, undef) = split(/-/, $login_token);
        # Regenerate the token based on the information we have.
        my $expected_token = issue_hash_token(['login_request', $login_cookie], $time);
        $valid = 1 if $expected_token eq $login_token;
        $cgi->remove_cookie('Bugzilla_login_request_cookie');
    }
    # WebServices and other local scripts can bypass this check.
    # This is safe because we won't store a login cookie in this case.
    elsif (Bugzilla->usage_mode != USAGE_MODE_BROWSER) {
        $valid = 1;
    }
    # Else falls back to the Referer header and accept local URLs.
    # Attachments are served from a separate host (ideally), and so
    # an evil attachment cannot abuse this check with a redirect.
    elsif (my $referer = $cgi->referer) {
        my $urlbase = correct_urlbase();
        $valid = 1 if $referer =~ /^\Q$urlbase\E/;
    }
    # If the web browser doesn't accept cookies and the Referer header
    # is missing, we have no way to make sure that the authentication
    # request comes from the user.
    elsif ($login && $password) {
        ThrowUserError('auth_untrusted_request', { login => $login });
    }

    if (!defined($login) || !defined($password) || !$valid) {
        return { failure => AUTH_NODATA };
    }

    return { username => $login, password => $password };
}

sub fail_nodata {
    my ($self) = @_;
    my $cgi = Bugzilla->cgi;
    my $template = Bugzilla->template;

    if (Bugzilla->usage_mode != USAGE_MODE_BROWSER) {
        ThrowUserError('login_required');
    }

    print $cgi->header();
    $template->process("account/auth/login.html.tmpl",
                       { 'target' => $cgi->url(-relative=>1) }) 
        || ThrowTemplateError($template->error());
    exit;
}

1;
