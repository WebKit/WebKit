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
# The Initial Developer of the Original Code is Frédéric Buclin.
# Portions created by Frédéric Buclin are Copyright (C) 2007
# Frédéric Buclin. All Rights Reserved.
#
# Contributor(s): Frédéric Buclin <LpSolit@gmail.com>

use strict;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('admin')
  || $user->in_group('tweakparams')
  || $user->in_group('editusers')
  || $user->can_bless
  || (Bugzilla->params->{'useclassification'} && $user->in_group('editclassifications'))
  || $user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || $user->in_group('creategroups')
  || $user->in_group('editkeywords')
  || $user->in_group('bz_canusewhines')
  || ThrowUserError('auth_failure', {action => 'access', object => 'administrative_pages'});

$template->process('admin/admin.html.tmpl')
  || ThrowTemplateError($template->error());
