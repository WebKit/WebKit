# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::GitHubAuth::Client;

use 5.10.1;
use strict;
use warnings;

use JSON qw(decode_json);
use URI;
use URI::QueryParam;
use Digest;
use LWP::UserAgent;

use Bugzilla::Extension::GitHubAuth::Client::Error qw(ThrowUserError ThrowCodeError);
use Bugzilla::Util qw(remote_ip correct_urlbase);

use constant DIGEST_HASH => 'SHA1';

use fields qw(user_agent);

use constant {
    GH_ACCESS_TOKEN_URI => 'https://github.com/login/oauth/access_token',
    GH_AUTHORIZE_URI    => 'https://github.com/login/oauth/authorize',
    GH_USER_EMAILS_URI  => 'https://api.github.com/user/emails',
};

sub new {
    my ($class, %init) = @_;
    my $self = $class->fields::new();

    return $self;
}

sub login_uri {
    my ($class, $target_uri) = @_;

    my $uri = URI->new_abs("github.cgi", correct_urlbase());
    $uri->query_form(target_uri => $target_uri);
    return $uri;
}

sub authorize_uri {
    my ($class, $state) = @_;

    my $uri = URI->new(GH_AUTHORIZE_URI);
    $uri->query_form(
        client_id    => Bugzilla->params->{github_client_id},
        scope        => 'user:email',
        state        => $state,
        redirect_uri => URI->new_abs("github.cgi", correct_urlbase()),
    );

    return $uri;
}

sub get_email_key {
    my ($class, $email) = @_;

    my $cgi    = Bugzilla->cgi;
    my $digest = Digest->new(DIGEST_HASH);
    $digest->add($email);
    $digest->add(remote_ip());
    $digest->add($cgi->cookie('Bugzilla_github_token') // Bugzilla->request_cache->{github_token} // '');
    $digest->add(Bugzilla->localconfig->{site_wide_secret});
    return $digest->hexdigest;
}

sub _handle_response {
    my ($self, $response) = @_;

    my $data = eval {
        decode_json($response->content);
    };
    if ($@) {
        ThrowCodeError("github_bad_response", { message => "Unable to parse json response: " . $response->content  });
    }

    unless ($response->is_success) {
        ThrowCodeError("github_error", { response => $response });
    }
    return $data;
}

sub get_access_token {
    my ($self, $code) = @_;

    my $response = $self->user_agent->post(
        GH_ACCESS_TOKEN_URI,
        { client_id     => Bugzilla->params->{github_client_id},
          client_secret => Bugzilla->params->{github_client_secret},
          code          => $code },
        Accept => 'application/json',
    );

    my $data = $self->_handle_response($response);
    return $data->{access_token} if exists $data->{access_token};
}

sub get_user_emails {
    my ($self, $access_token) = @_;

    my $uri = URI->new(GH_USER_EMAILS_URI);

    my $response = $self->user_agent->get($uri, Accept => 'application/json', Authorization => 'token ' . $access_token);
    return $self->_handle_response($response);
}

sub user_agent {
    my ($self) = @_;
    $self->{user_agent} //= $self->_build_user_agent;

    return $self->{user_agent};
}

sub _build_user_agent {
    my ($self) = @_;
    my $ua = LWP::UserAgent->new( timeout => 10 );

    if (Bugzilla->params->{proxy_url}) {
        $ua->proxy('https', Bugzilla->params->{proxy_url});
    }

    return $ua;
}

1;
