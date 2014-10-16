#!/usr/bin/env perl -wT
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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Joe Robins <jmrobins@tgix.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>

use strict;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Token;

# Just in case someone already has an account, let them get the correct footer
# on an error message. The user is logged out just after the account is
# actually created.
my $user = Bugzilla->login(LOGIN_OPTIONAL);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = { doc_section => 'myaccount.html' };

print $cgi->header();

$user->check_account_creation_enabled;
my $login = $cgi->param('login');

if (defined($login)) {
    # Check the hash token to make sure this user actually submitted
    # the create account form.
    my $token = $cgi->param('token');
    check_hash_token($token, ['create_account']);

    $user->check_and_send_account_creation_confirmation($login);
    $vars->{'login'} = $login;

    $template->process("account/created.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# Show the standard "would you like to create an account?" form.
$template->process("account/create.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
