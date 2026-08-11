# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::GitHubAuth::Login;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::Auth::Login);
use fields qw(github_failure);

use Scalar::Util qw(blessed);

use Bugzilla::Constants qw(AUTH_NODATA AUTH_ERROR USAGE_MODE_BROWSER);
use Bugzilla::Util qw(trick_taint correct_urlbase generate_random_password);
use Bugzilla::Token qw(issue_short_lived_session_token set_token_extra_data);
use List::MoreUtils qw(any);
use Bugzilla::Extension::GitHubAuth::Client;
use Bugzilla::Extension::GitHubAuth::Client::Error ();
use Bugzilla::Error;

use constant { requires_verification   => 1,
               is_automatic            => 1,
               user_can_create_account => 1 };

sub get_login_info {
    my ($self) = @_;
    my $cgi              = Bugzilla->cgi;
    my $github_action    = Bugzilla->request_cache->{github_action};
    
    return { failure => AUTH_NODATA } unless $github_action;

    my $response;
    if ($github_action eq 'login') {
        $response = $self->_get_login_info_from_github();
    }
    elsif ($github_action eq 'email') {
        $response = $self->_get_login_info_from_email();
    }

    if (!exists $response->{failure}) {
        if (exists $response->{user}) {
            # existing account
            my $user = $response->{user};
            return { failure    => AUTH_ERROR,
                    user_error => 'github_auth_account_too_powerful' } if $user->in_group('no-github-auth');
            $response = {
                username    => $user->login,
                user_id     => $user->id,
                github_auth => 1,
            };
        }
        else {
            # new account
            my $email = $response->{email};
            $response = {
                username    => $email,
                github_auth => 1,
            };
        }
    }
    return $response;
}

sub _get_login_info_from_github {
    my ($self) = @_;
    my $cgi      = Bugzilla->cgi;
    my $template = Bugzilla->template;
    my $code     = $cgi->param('code');

    return { failure => AUTH_ERROR, error => 'github_missing_code' } unless $code;

    trick_taint($code);

    my $client = Bugzilla::Extension::GitHubAuth::Client->new;

    my ($access_token, $emails);
    eval {
        # The following variable lets us catch and return (rather than throw) errors
        # from our github client code, as required by the Auth API.
        local $Bugzilla::Extension::GitHubAuth::Client::Error::USE_EXCEPTION_OBJECTS = 1;
        $access_token = $client->get_access_token($code);
        $emails       = $client->get_user_emails($access_token);
    };
    my $e = $@;
    if (blessed $e && $e->isa('Bugzilla::Extension::GitHubAuth::Client::Error')) {
        my $key = $e->type eq 'user' ? 'user_error' : 'error';
        return { failure => AUTH_ERROR, $key => $e->error, details => $e->vars };
    }
    elsif ($e) {
        die $e;
    }

    my @emails = map { $_->{email} }
                 grep { $_->{verified} && $_->{email} !~ /\@users\.noreply\.github\.com$/ } @$emails;

    my @bugzilla_users;
    my @github_emails;
    foreach my $email (@emails) {
        my $user = Bugzilla::User->new({name => $email, cache => 1});
        if ($user) {
            push @bugzilla_users, $user;
        }
        else {
            push @github_emails, $email;
        }
    }
    my @allowed_bugzilla_users = grep { not $_->in_group('no-github-auth') } @bugzilla_users;
    
    if (@allowed_bugzilla_users == 1) {
        my ($user) = @allowed_bugzilla_users;
        return { user => $user };
    }
    elsif (@allowed_bugzilla_users > 1) {
        $self->{github_failure} = {
            template => 'account/auth/github-verify-account.html.tmpl',
            vars     => {
                bugzilla_users => \@allowed_bugzilla_users,
                choose_email   => _mk_choose_email(\@emails),
            },
        };
        return { failure => AUTH_NODATA };
    }
    elsif (@allowed_bugzilla_users == 0 && @bugzilla_users > 0 && @github_emails == 0) {
            return { failure    => AUTH_ERROR,
                     user_error => 'github_auth_account_too_powerful' };
    }
    elsif (@github_emails) {

        $self->{github_failure} = {
            template => 'account/auth/github-verify-account.html.tmpl',
            vars     => {
                github_emails => \@github_emails,
                choose_email   => _mk_choose_email(\@emails),
            },
        };
        return { failure => AUTH_NODATA };
    }
    else {
        return { failure => AUTH_ERROR, user_error => 'github_no_emails' };
    }
}

sub _get_login_info_from_email {
    my ($self) = @_;
    my $cgi = Bugzilla->cgi;
    my $email = $cgi->param('email') or return { failure => AUTH_ERROR,
                                                 user_error => 'github_invalid_email',
                                                 details => { email => '' } };
    trick_taint($email);

    unless (any { $_ eq $email } @{ Bugzilla->request_cache->{github_emails} }) {
        return { failure    => AUTH_ERROR,
                 user_error => 'github_invalid_email',
                 details    => { email => $email }};
    }

    my $user = Bugzilla::User->new({name => $email, cache => 1});
    $cgi->remove_cookie('Bugzilla_github_token');
    return $user ? { user => $user } : { email => $email };
}

sub fail_nodata {
    my ($self) = @_;
    my $cgi      = Bugzilla->cgi;
    my $template = Bugzilla->template;

    ThrowUserError('login_required') if Bugzilla->usage_mode != USAGE_MODE_BROWSER;

    my $file = $self->{github_failure}{template} // "account/auth/login.html.tmpl";
    my $vars = $self->{github_failure}{vars} // { target => $cgi->url(-relative=>1) };

    print $cgi->header();
    $template->process($file, $vars) or ThrowTemplateError($template->error());
    exit;
}

sub _store_emails {
    my ($emails) = @_;
    my $state = issue_short_lived_session_token("github_email");
    set_token_extra_data($state, { type       => 'github_email',
                                   emails     => $emails,
                                   target_uri => Bugzilla->request_cache->{github_target_uri} });

    Bugzilla->cgi->send_cookie(-name     => 'github_state',
                               -value    => $state,
                               -httponly => 1);
    return $state;
}

sub _mk_choose_email {
    my ($emails) = @_;
    my $state = _store_emails($emails);

    return sub {
        my $email = shift;
        my $uri = URI->new_abs("github.cgi", correct_urlbase());
        $uri->query_form( state => $state, email => $email );
        return $uri;
    };
}

1;
