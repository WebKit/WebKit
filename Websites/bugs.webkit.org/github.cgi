#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib local/lib/perl5);

use Bugzilla;
use Bugzilla::Util qw( correct_urlbase );
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Token qw( issue_short_lived_session_token
                        set_token_extra_data
                        get_token_extra_data
                        delete_token );
use URI;
use URI::QueryParam;
BEGIN { Bugzilla->extensions }
use Bugzilla::Extension::GitHubAuth::Client;

my $cgi     = Bugzilla->cgi;
my $urlbase = correct_urlbase();

if (lc($cgi->request_method) eq 'post') {
    # POST requests come from Bugzilla itself and begin the GitHub login process
    # by redirecting the user to GitHub's authentication endpoint.

    my $user           = Bugzilla->login(LOGIN_OPTIONAL);
    my $target_uri     = $cgi->param('target_uri')    or ThrowCodeError("github_invalid_target");
    my $github_secret  = $cgi->param('github_secret') or ThrowCodeError("github_invalid_request", { reason => 'invalid secret' });
    my $github_secret2 = Bugzilla->github_secret      or ThrowCodeError("github_invalid_request", { reason => 'invalid secret' });

    ThrowCodeError("github_invalid_request", { reason => 'invalid secret' })
      unless $github_secret eq $github_secret2;

    ThrowCodeError("github_invalid_target", { target_uri => $target_uri })
      unless $target_uri =~ /^\Q$urlbase\E/;

    ThrowCodeError("github_insecure_referer", { target_uri => $target_uri })
      if $cgi->referer && $cgi->referer =~ /(reset_password\.cgi|token\.cgi|\bt=|token=|api_key=)/;

    if ($user->id) {
        print $cgi->redirect($target_uri);
        exit;
    }

    my $state = issue_short_lived_session_token("github_state");
    set_token_extra_data($state, { type => 'github_login', target_uri => $target_uri });

    $cgi->send_cookie(-name     => 'github_state',
                      -value    => $state,
                      -httponly => 1);
    print $cgi->redirect(Bugzilla::Extension::GitHubAuth::Client->authorize_uri($state));
}
elsif (lc($cgi->request_method) eq 'get') {
    # GET requests come from GitHub, with this script acting as the OAuth2 callback.
    my $state_param  = $cgi->param('state');
    my $state_cookie = $cgi->cookie('github_state');

    # If the state or params are missing, or the github_state cookie is missing
    # we just redirect to index.cgi.
    unless ($state_param && $state_cookie && ($cgi->param('code') || $cgi->param('email'))) {
        print $cgi->redirect(URI->new_abs("index.cgi", $urlbase));
        exit;
    }

    ThrowCodeError("github_invalid_request", { reason => 'invalid state param' })
      unless $state_param eq $state_cookie;

    my $state_data = get_token_extra_data($state_param);
    ThrowCodeError("github_invalid_request", { reason => 'invalid state param' } )
      unless $state_data && $state_data->{type};

    $cgi->remove_cookie('github_state');
    delete_token($state_param);

    if ($state_data->{type} eq 'github_login') {
        Bugzilla->request_cache->{github_action}     = 'login';
        Bugzilla->request_cache->{github_target_uri} = $state_data->{target_uri};
    }
    elsif ($state_data->{type} eq 'github_email') {
        Bugzilla->request_cache->{github_action} = 'email';
        Bugzilla->request_cache->{github_emails} = $state_data->{emails};
    }
    else {
        ThrowCodeError("github_invalid_request", { reason => "invalid state param" })
    }
    my $user = Bugzilla->login(LOGIN_REQUIRED);

    my $target_uri = URI->new($state_data->{target_uri});
    warn("Target URI: $target_uri");
        
    # It makes very little sense to login to a page with the logout parameter.
    # doing so would be a no-op, so we ignore the logout param here.
    $target_uri->query_param_delete('logout');

    if ($target_uri->path =~ /attachment\.cgi/) {
        my $attachment_uri = URI->new_abs("attachment.cgi", $urlbase);
        $attachment_uri->query_param(id => scalar $target_uri->query_param('id'));
        if ($target_uri->query_param('action')) {
            $attachment_uri->query_param(action => scalar $target_uri->query_param('action'));
        }
        print $cgi->redirect($attachment_uri);
    }
    else {
        print $cgi->redirect($target_uri);
    }
}
