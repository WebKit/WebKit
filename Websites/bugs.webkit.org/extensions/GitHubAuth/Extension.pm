# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::GitHubAuth;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use Bugzilla::Extension::GitHubAuth::Client;

use Bugzilla::Error;
use Bugzilla::Util qw(trick_taint);
use List::Util qw(first);
use URI;
use URI::QueryParam;

our $VERSION = '0.01';

BEGIN {
    # Monkey-patch can() on Bugzilla::Auth::Login::CGI so that our own fail_nodata gets called.
    # Our fail_nodata behaves like CGI's, so this shouldn't be a problem for CGI-based logins.

    *Bugzilla::Auth::Login::CGI::can = sub {
        my ($stack, $method) = @_;

        return undef if $method eq 'fail_nodata';
        return $stack->SUPER::can($method);
    };
}

sub install_before_final_checks {
    Bugzilla::Group->create({
        name        => 'no-github-auth',
        description => 'Group containing groups whose members may not use GitHubAuth to log in',
        isbuggroup  => 0,
    }) unless Bugzilla::Group->new({ name => 'no-github-auth' });
}

sub attachment_should_redirect_login {
    my ($self, $args) = @_;
    my $cgi = Bugzilla->cgi;

    if ($cgi->param('github_state') || $cgi->param('github_email')) {
        ${$args->{do_redirect}} = 1;
    }
}

sub auth_login_methods {
    my ($self, $args) = @_;
    my $modules = $args->{'modules'};
    if (exists $modules->{'GitHubAuth'}) {
        $modules->{'GitHubAuth'} = 'Bugzilla/Extension/GitHubAuth/Login.pm';
    }
}

sub auth_verify_methods {
    my ($self, $args) = @_;
    my $modules = $args->{'modules'};
    if (exists $modules->{'GitHubAuth'}) {
        $modules->{'GitHubAuth'} = 'Bugzilla/Extension/GitHubAuth/Verify.pm';
    }
}

sub config_modify_panels {
    my ($self, $args) = @_;
    my $auth_panel_params = $args->{panels}{auth}{params};

    my $user_info_class = first { $_->{name} eq 'user_info_class' } @$auth_panel_params;
    if ($user_info_class) {
        push @{ $user_info_class->{choices} }, "GitHubAuth,CGI", "Persona,GitHubAuth,CGI";
    }

    my $user_verify_class = first { $_->{name} eq 'user_verify_class' } @$auth_panel_params;
    if ($user_verify_class) {
        unshift @{ $user_verify_class->{choices} }, "GitHubAuth";
    }
}

sub config_add_panels {
    my ($self, $args) = @_;
    my $modules = $args->{panel_modules};
    $modules->{GitHubAuth} = "Bugzilla::Extension::GitHubAuth::Config";
}

__PACKAGE__->NAME;
