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

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Mailer;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Token;
use Bugzilla::User;
use Bugzilla::Util;
use Date::Format;

my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

my $action = $cgi->param('action') || '';

my $vars = {};
my $target;

if (!$action) {
    # redirect to index.cgi if no action is defined.
    print $cgi->redirect(correct_urlbase() . 'index.cgi');
    exit;
}
# prepare-sudo: Display the sudo information & login page
elsif ($action eq 'prepare-sudo') {
    # We must have a logged-in user to do this
    # That user must be in the 'bz_sudoers' group
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    unless ($user->in_group('bz_sudoers')) {
        ThrowUserError('auth_failure', {  group => 'bz_sudoers',
                                         action => 'begin',
                                         object => 'sudo_session' }
        );
    }
    
    # Do not try to start a new session if one is already in progress!
    if (defined(Bugzilla->sudoer)) {
        ThrowUserError('sudo_in_progress', { target => $user->login });
    }

    # Keep a temporary record of the user visiting this page
    $vars->{'token'} = issue_session_token('sudo_prepared');

    if ($user->authorizer->can_login) {
        my $value = generate_random_password();
        my %args;
        $args{'-secure'} = 1 if Bugzilla->params->{ssl_redirect};

        $cgi->send_cookie(-name => 'Bugzilla_login_request_cookie',
                          -value => $value,
                          -httponly => 1,
                          %args);

        # The user ID must not be set when generating the token, because
        # that information will not be available when validating it.
        local Bugzilla->user->{userid} = 0;
        $vars->{'login_request_token'} = issue_hash_token(['login_request', $value]);
    }

    # Show the sudo page
    $vars->{'target_login_default'} = $cgi->param('target_login');
    $vars->{'reason_default'} = $cgi->param('reason');
    $target = 'admin/sudo.html.tmpl';
}
# begin-sudo: Confirm login and start sudo session
elsif ($action eq 'begin-sudo') {
    # We must be sure that the user is authenticating by providing a login
    # and password.
    # We only need to do this for authentication methods that involve Bugzilla 
    # directly obtaining a login (i.e. normal CGI login), as opposed to other 
    # methods (like Environment vars login). 

    # First, record if Bugzilla_login and Bugzilla_password were provided
    my $credentials_provided;
    if (defined($cgi->param('Bugzilla_login'))
        && defined($cgi->param('Bugzilla_password')))
    {
        $credentials_provided = 1;
    }

    # Next, log in the user
    my $user = Bugzilla->login(LOGIN_REQUIRED);

    my $target_login = $cgi->param('target_login');
    my $reason = $cgi->param('reason') || '';

    # At this point, the user is logged in.  However, if they used a method
    # where they could have provided a username/password (i.e. CGI), but they 
    # did not provide a username/password, then throw an error.
    if ($user->authorizer->can_login && !$credentials_provided) {
        ThrowUserError('sudo_password_required',
                       { target_login => $target_login, reason => $reason });
    }

    # The user must be in the 'bz_sudoers' group
    unless ($user->in_group('bz_sudoers')) {
        ThrowUserError('auth_failure', {  group => 'bz_sudoers',
                                         action => 'begin',
                                         object => 'sudo_session' }
        );
    }
    
    # Do not try to start a new session if one is already in progress!
    if (defined(Bugzilla->sudoer)) {
        ThrowUserError('sudo_in_progress', { target => $user->login });
    }

    # Did the user actually go trough the 'sudo-prepare' action?  Do some 
    # checks on the token the action should have left.
    my ($token_user, $token_timestamp, $token_data) =
        Bugzilla::Token::GetTokenData($cgi->param('token'));
    unless (defined($token_user)
            && defined($token_data)
            && ($token_user == $user->id)
            && ($token_data eq 'sudo_prepared'))
    {
        ThrowUserError('sudo_preparation_required', 
                       { target_login => $target_login, reason => $reason });
    }
    delete_token($cgi->param('token'));

    # Get & verify the target user (the user who we will be impersonating)
    my $target_user = new Bugzilla::User({ name => $target_login });
    unless (defined($target_user)
            && $target_user->id
            && $user->can_see_user($target_user))
    {
        ThrowUserError('user_match_failed', { name => $target_login });
    }
    if ($target_user->in_group('bz_sudo_protect')) {
        ThrowUserError('sudo_protected', { login => $target_user->login });
    }

    # Calculate the session expiry time (T + 6 hours)
    my $time_string = time2str('%a, %d-%b-%Y %T %Z', time + MAX_SUDO_TOKEN_AGE, 'GMT');

    # For future sessions, store the unique ID of the target user
    my $token = Bugzilla::Token::_create_token($user->id, 'sudo', $target_user->id);

    my %args;
    if (Bugzilla->params->{ssl_redirect}) {
        $args{'-secure'} = 1;
    }

    $cgi->send_cookie('-name'    => 'sudo',
                      '-expires' => $time_string,
                      '-value'   => $token,
                      '-httponly' => 1,
                      %args);

    # For the present, change the values of Bugzilla::user & Bugzilla::sudoer
    Bugzilla->sudo_request($target_user, $user);

    # NOTE: If you want to log the start of an sudo session, do it here.

    # If we have a reason passed in, keep it under 200 characters
    $reason = substr($reason, 0, 200);

    # Go ahead and send out the message now
    my $message;
    my $mail_template = Bugzilla->template_inner($target_user->setting('lang'));
    $mail_template->process('email/sudo.txt.tmpl', { reason => $reason }, \$message);
    MessageToMTA($message);

    $vars->{'message'} = 'sudo_started';
    $vars->{'target'} = $target_user->login;
    $target = 'global/message.html.tmpl';
}
# end-sudo: End the current sudo session (if one is in progress)
elsif ($action eq 'end-sudo') {
    # Regardless of our state, delete the sudo cookie if it exists
    my $token = $cgi->cookie('sudo');
    $cgi->remove_cookie('sudo');

    # Are we in an sudo session?
    Bugzilla->login(LOGIN_OPTIONAL);
    my $sudoer = Bugzilla->sudoer;
    if (defined($sudoer)) {
        Bugzilla->sudo_request($sudoer, undef);
    }
    # Now that the session is over, remove the token from the DB.
    delete_token($token);

    # NOTE: If you want to log the end of an sudo session, so it here.
    
    $vars->{'message'} = 'sudo_ended';
    $target = 'global/message.html.tmpl';
}
# No valid action found
else {
    Bugzilla->login(LOGIN_OPTIONAL);
    ThrowUserError('unknown_action', {action => $action});
}

# Display the template
print $cgi->header();
$template->process($target, $vars)
      || ThrowTemplateError($template->error());
