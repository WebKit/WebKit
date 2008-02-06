#!/usr/bin/perl -wT
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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>

############################################################################
# Script Initialization
############################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(.);

use vars qw($template $vars);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

Bugzilla->login(LOGIN_OPTIONAL);

# Use the "Bugzilla::Token" module that contains functions for doing various
# token-related tasks.
use Bugzilla::Token;

use Bugzilla::User;

################################################################################
# Data Validation / Security Authorization
################################################################################

# Throw an error if the form does not contain an "action" field specifying
# what the user wants to do.
$cgi->param('a') || ThrowCodeError("unknown_action");

# Assign the action to a global variable.
$::action = $cgi->param('a');

# If a token was submitted, make sure it is a valid token that exists in the
# database and is the correct type for the action being taken.
if ($cgi->param('t')) {
  # Assign the token and its SQL quoted equivalent to global variables.
  $::token = $cgi->param('t');
  $::quotedtoken = SqlQuote($::token);
  
  # Make sure the token contains only valid characters in the right amount.
  # Validate password will throw an error if token is invalid
  ValidatePassword($::token);

  Bugzilla::Token::CleanTokenTable();

  # Make sure the token exists in the database.
  SendSQL( "SELECT tokentype FROM tokens WHERE token = $::quotedtoken" );
  (my $tokentype = FetchSQLData()) || ThrowUserError("token_inexistent");

  # Make sure the token is the correct type for the action being taken.
  if ( grep($::action eq $_ , qw(cfmpw cxlpw chgpw)) && $tokentype ne 'password' ) {
    Bugzilla::Token::Cancel($::token, "wrong_token_for_changing_passwd");
    ThrowUserError("wrong_token_for_changing_passwd");
  }
  if ( ($::action eq 'cxlem') 
      && (($tokentype ne 'emailold') && ($tokentype ne 'emailnew')) ) {
    Bugzilla::Token::Cancel($::token, "wrong_token_for_cancelling_email_change");
    ThrowUserError("wrong_token_for_cancelling_email_change");
  }
  if ( grep($::action eq $_ , qw(cfmem chgem)) 
      && ($tokentype ne 'emailnew') ) {
    Bugzilla::Token::Cancel($::token, "wrong_token_for_confirming_email_change");
    ThrowUserError("wrong_token_for_confirming_email_change");
  }
}


# If the user is requesting a password change, make sure they submitted
# their login name and it exists in the database, and that the DB module is in
# the list of allowed verification methids.
if ( $::action eq 'reqpw' ) {
    defined $cgi->param('loginname')
      || ThrowUserError("login_needed_for_password_change");

    # check verification methods
    unless (Bugzilla::Auth->has_db) {
        ThrowUserError("password_change_requests_not_allowed");
    }

    # Make sure the login name looks like an email address.  This function
    # displays its own error and stops execution if the login name looks wrong.
    CheckEmailSyntax($cgi->param('loginname'));

    my $quotedloginname = SqlQuote($cgi->param('loginname'));
    SendSQL("SELECT userid FROM profiles WHERE " .
            $dbh->sql_istrcmp('login_name', $quotedloginname));
    FetchSQLData()
      || ThrowUserError("account_inexistent");
}

# If the user is changing their password, make sure they submitted a new
# password and that the new password is valid.
if ( $::action eq 'chgpw' ) {
    defined $cgi->param('password')
      && defined $cgi->param('matchpassword')
      || ThrowUserError("require_new_password");

    ValidatePassword($cgi->param('password'), $cgi->param('matchpassword'));
}

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

if ($::action eq 'reqpw') { 
    requestChangePassword(); 
} elsif ($::action eq 'cfmpw') { 
    confirmChangePassword(); 
} elsif ($::action eq 'cxlpw') { 
    cancelChangePassword(); 
} elsif ($::action eq 'chgpw') { 
    changePassword(); 
} elsif ($::action eq 'cfmem') {
    confirmChangeEmail();
} elsif ($::action eq 'cxlem') {
    cancelChangeEmail();
} elsif ($::action eq 'chgem') {
    changeEmail();
} else { 
    # If the action that the user wants to take (specified in the "a" form field)
    # is none of the above listed actions, display an error telling the user 
    # that we do not understand what they would like to do.
    ThrowCodeError("unknown_action", { action => $::action });
}

exit;

################################################################################
# Functions
################################################################################

sub requestChangePassword {
    Bugzilla::Token::IssuePasswordToken($cgi->param('loginname'));

    $vars->{'message'} = "password_change_request";

    print $cgi->header();
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub confirmChangePassword {
    $vars->{'token'} = $::token;
    
    print $cgi->header();
    $template->process("account/password/set-forgotten-password.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub cancelChangePassword {    
    $vars->{'message'} = "password_change_cancelled";
    Bugzilla::Token::Cancel($::token, $vars->{'message'});

    print $cgi->header();
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub changePassword {
    my $dbh = Bugzilla->dbh;

    # Quote the password and token for inclusion into SQL statements.
    my $cryptedpassword = bz_crypt($cgi->param('password'));
    my $quotedpassword = SqlQuote($cryptedpassword);

    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid FROM tokens WHERE token = $::quotedtoken");
    my $userid = FetchSQLData();
    
    # Update the user's password in the profiles table and delete the token
    # from the tokens table.
    $dbh->bz_lock_tables('profiles WRITE', 'tokens WRITE');
    SendSQL("UPDATE   profiles
             SET      cryptpassword = $quotedpassword
             WHERE    userid = $userid");
    SendSQL("DELETE FROM tokens WHERE token = $::quotedtoken");
    $dbh->bz_unlock_tables();

    Bugzilla->logout_user_by_id($userid);

    $vars->{'message'} = "password_changed";

    print $cgi->header();
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub confirmChangeEmail {
    # Return HTTP response headers.
    print $cgi->header();

    $vars->{'token'} = $::token;

    $template->process("account/email/confirm.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub changeEmail {
    my $dbh = Bugzilla->dbh;

    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid, eventdata FROM tokens 
              WHERE token = $::quotedtoken");
    my ($userid, $eventdata) = FetchSQLData();
    my ($old_email, $new_email) = split(/:/,$eventdata);
    my $quotednewemail = SqlQuote($new_email);

    # Check the user entered the correct old email address
    if(lc($cgi->param('email')) ne lc($old_email)) {
        ThrowUserError("email_confirmation_failed");
    }
    # The new email address should be available as this was 
    # confirmed initially so cancel token if it is not still available
    if (! is_available_username($new_email,$old_email)) {
        $vars->{'email'} = $new_email; # Needed for Bugzilla::Token::Cancel's mail
        Bugzilla::Token::Cancel($::token,"account_exists");
        ThrowUserError("account_exists", { email => $new_email } );
    } 

    # Update the user's login name in the profiles table and delete the token
    # from the tokens table.
    $dbh->bz_lock_tables('profiles WRITE', 'tokens WRITE');
    SendSQL("UPDATE   profiles
         SET      login_name = $quotednewemail
         WHERE    userid = $userid");
    SendSQL("DELETE FROM tokens WHERE token = $::quotedtoken");
    SendSQL("DELETE FROM tokens WHERE userid = $userid 
                                  AND tokentype = 'emailnew'");
    $dbh->bz_unlock_tables();

    # The email address has been changed, so we need to rederive the groups
    my $user = new Bugzilla::User($userid);
    $user->derive_groups;

    # Return HTTP response headers.
    print $cgi->header();

    # Let the user know their email address has been changed.

    $vars->{'message'} = "login_changed";

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub cancelChangeEmail {
    my $dbh = Bugzilla->dbh;

    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid, tokentype, eventdata FROM tokens 
             WHERE token = $::quotedtoken");
    my ($userid, $tokentype, $eventdata) = FetchSQLData();
    my ($old_email, $new_email) = split(/:/,$eventdata);

    if($tokentype eq "emailold") {
        $vars->{'message'} = "emailold_change_cancelled";

        SendSQL("SELECT login_name FROM profiles WHERE userid = $userid");
        my $actualemail = FetchSQLData();
        
        # check to see if it has been altered
        if($actualemail ne $old_email) {
            my $quotedoldemail = SqlQuote($old_email);

            $dbh->bz_lock_tables('profiles WRITE');
            SendSQL("UPDATE   profiles
                 SET      login_name = $quotedoldemail
                 WHERE    userid = $userid");
            $dbh->bz_unlock_tables();

            # email has changed, so rederive groups
            # Note that this is done _after_ the tables are unlocked
            # This is sort of a race condition (given the lack of transactions)
            # but the user had access to it just now, so it's not a security
            # issue

            my $user = new Bugzilla::User($userid);
            $user->derive_groups;

            $vars->{'message'} = "email_change_cancelled_reinstated";
        } 
    } 
    else {
        $vars->{'message'} = 'email_change_cancelled'
     }

    $vars->{'old_email'} = $old_email;
    $vars->{'new_email'} = $new_email;
    Bugzilla::Token::Cancel($::token, $vars->{'message'});

    $dbh->bz_lock_tables('tokens WRITE');
    SendSQL("DELETE FROM tokens 
             WHERE userid = $userid 
             AND tokentype = 'emailold' OR tokentype = 'emailnew'");
    $dbh->bz_unlock_tables();

    # Return HTTP response headers.
    print $cgi->header();

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

