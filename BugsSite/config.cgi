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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# Include the Bugzilla CGI and general utility library.
use lib qw(.);
require "CGI.pl";
use Bugzilla;
use Bugzilla::Constants;

# Suppress "used only once" warnings.
use vars 
  qw(
    @legal_priority 
    @legal_severity 
    @legal_platform 
    @legal_opsys 
    @legal_resolution 

    @legal_components 
    @legal_target_milestone 
    @legal_versions 
    @legal_keywords 
  );

# Use the global template variables defined in globals.pl 
# to generate the output.
use vars qw($template $vars);

Bugzilla->login(LOGIN_OPTIONAL);

# If the 'requirelogin' parameter is on and the user is not
# authenticated, return empty fields.
if (Param('requirelogin') && !Bugzilla->user->id) {
    display_data();
}

# Retrieve this installation's configuration.
GetVersionTable();

# Pass a bunch of Bugzilla configuration to the templates.
$vars->{'priority'}  = \@::legal_priority;
$vars->{'severity'}  = \@::legal_severity;
$vars->{'platform'}   = \@::legal_platform;
$vars->{'op_sys'}    = \@::legal_opsys;
$vars->{'keyword'}    = \@::legal_keywords;
$vars->{'resolution'} = \@::legal_resolution;
$vars->{'status'}    = \@::legal_bug_status;

# Include lists of products, components, versions, and target milestones.
my $selectables = GetSelectableProductHash();
foreach my $selectable (keys %$selectables) {
    $vars->{$selectable} = $selectables->{$selectable};
}

# Create separate lists of open versus resolved statuses.  This should really
# be made part of the configuration.
my @open_status;
my @closed_status;
foreach my $status (@::legal_bug_status) {
    IsOpenedState($status) ? push(@open_status, $status) 
                           : push(@closed_status, $status);
}
$vars->{'open_status'} = \@open_status;
$vars->{'closed_status'} = \@closed_status;

# Generate a list of fields that can be queried.
$vars->{'field'} = [Bugzilla->dbh->bz_get_field_defs()];

display_data($vars);


sub display_data {
    my $vars = shift;

    my $cgi = Bugzilla->cgi;
    # Determine how the user would like to receive the output; 
    # default is JavaScript.
    my $format = GetFormat("config", scalar($cgi->param('format')),
                           scalar($cgi->param('ctype')) || "js");

    # Return HTTP headers.
    print "Content-Type: $format->{'ctype'}\n\n";

    # Generate the configuration file and return it to the user.
    $template->process($format->{'template'}, $vars)
      || ThrowTemplateError($template->error());
    exit;
}
