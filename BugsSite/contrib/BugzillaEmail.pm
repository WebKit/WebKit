# -*- Mode: perl; indent-tabs-mode: nil -*-

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
# This code is based on code found in bug_email.pl from the bugzilla
# email tracker.  Initial contributors are :: 
#                 Terry Weissman <terry@mozilla.org>
#                 Gregor Fischer <fischer@suse.de>
#                 Klaas Freitag  <freitag@suse.de>
#                 Seth Landsman  <seth@dworkin.net>

# The purpose of this module is to abstract out a bunch of the code
#  that is central to email interfaces to bugzilla and its database

# Contributor : Seth Landsman <seth@dworkin.net>

# Initial checkin : 03/15/00 (SML)
#  findUser() function moved from bug_email.pl to here

push @INC, "../."; # this script now lives in contrib

require "globals.pl";

use strict;

my $dbh = Bugzilla->dbh;

my $EMAIL_TRANSFORM_NONE = "email_transform_none";
my $EMAIL_TRANSFORM_BASE_DOMAIN = "email_transform_base_domain";
my $EMAIL_TRANSFORM_NAME_ONLY = "email_transform_name_only";

# change to do incoming email address fuzzy matching
my $email_transform = $EMAIL_TRANSFORM_NAME_ONLY;

# findUser()
# This function takes an email address and returns the user email.
# matching is sloppy based on the $email_transform parameter
sub findUser($) {
  my ($address) = @_;
  # if $email_transform is $EMAIL_TRANSFORM_NONE, return the address, otherwise, return undef
  if ($email_transform eq $EMAIL_TRANSFORM_NONE) {
    my $stmt = "SELECT login_name FROM profiles WHERE " .
               $dbh->sql_istrcmp('login_name', $dbh->quote($address));
    SendSQL($stmt);
    my $found_address = FetchOneColumn();
    return $found_address;
  } elsif ($email_transform eq $EMAIL_TRANSFORM_BASE_DOMAIN) {
    my ($username) = ($address =~ /(.+)@/);
    my $stmt = "SELECT login_name FROM profiles WHERE " . $dbh->sql_istrcmp(
               'login_name', $dbh->quote($username), $dbh->sql_regexp());
    SendSQL($stmt);

    my $domain;
    my $found = undef;
    my $found_address;
    my $new_address = undef;
    while ((!$found) && ($found_address = FetchOneColumn())) {
      ($domain) = ($found_address =~ /.+@(.+)/);
      if ($address =~ /$domain/) {
        $found = 1;
        $new_address = $found_address;
      }
    }
    return $new_address;
  } elsif ($email_transform eq $EMAIL_TRANSFORM_NAME_ONLY) {
    my ($username) = ($address =~ /(.+)@/);
    my $stmt = "SELECT login_name FROM profiles WHERE " .$dbh->sql_istrcmp(
                'login_name', $dbh->quote($username), $dbh->sql_regexp());
    SendSQL($stmt);
    my $found_address = FetchOneColumn();
    return $found_address;
  }
}

1;
