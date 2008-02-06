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
# Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Erik Stambaugh <erik@dasbistro.com>
#

package Bugzilla;

use strict;

use Bugzilla::Auth;
use Bugzilla::Auth::Login::WWW;
use Bugzilla::CGI;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::DB;
use Bugzilla::Template;
use Bugzilla::User;

my $_template;
sub template {
    my $class = shift;
    $_template ||= Bugzilla::Template->create();
    return $_template;
}

my $_cgi;
sub cgi {
    my $class = shift;
    $_cgi ||= new Bugzilla::CGI();
    return $_cgi;
}

my $_user;
sub user {
    my $class = shift;

    if (not defined $_user) {
        $_user = new Bugzilla::User;
    }

    return $_user;
}

sub login {
    my ($class, $type) = @_;
    $_user = Bugzilla::Auth::Login::WWW->login($type);
}

sub logout {
    my ($class, $option) = @_;

    # If we're not logged in, go away
    return unless user->id;

    $option = LOGOUT_CURRENT unless defined $option;
    Bugzilla::Auth::Login::WWW->logout($_user, $option);
}

sub logout_user {
    my ($class, $user) = @_;
    # When we're logging out another user we leave cookies alone, and
    # therefore avoid calling Bugzilla->logout() directly.
    Bugzilla::Auth::Login::WWW->logout($user, LOGOUT_ALL);
}

# just a compatibility front-end to logout_user that gets a user by id
sub logout_user_by_id {
    my ($class, $id) = @_;
    my $user = new Bugzilla::User($id);
    $class->logout_user($user);
}

# hack that invalidates credentials for a single request
sub logout_request {
    undef $_user;
    # XXX clean this up eventually
    $::userid = 0;
    # We can't delete from $cgi->cookie, so logincookie data will remain
    # there. Don't rely on it: use Bugzilla->user->login instead!
}

my $_dbh;
my $_dbh_main;
my $_dbh_shadow;
sub dbh {
    my $class = shift;

    # If we're not connected, then we must want the main db
    if (!$_dbh) {
        $_dbh = $_dbh_main = Bugzilla::DB::connect_main();
    }

    return $_dbh;
}

my $_batch;
sub batch {
    my $class = shift;
    my $newval = shift;
    if ($newval) {
        $_batch = $newval;
    }
    return $_batch || 0;
}

sub dbwritesallowed {
    my $class = shift;

    # We can write if we are connected to the main database.
    # Note that if we don't have a shadowdb, then we claim that its ok
    # to write even if we're nominally connected to the shadowdb.
    # This is OK because this method is only used to test if misc
    # updates can be done, rather than anything complicated.
    return $class->dbh == $_dbh_main;
}

sub switch_to_shadow_db {
    my $class = shift;

    if (!$_dbh_shadow) {
        if (Param('shadowdb')) {
            $_dbh_shadow = Bugzilla::DB::connect_shadow();
        } else {
            $_dbh_shadow = $_dbh_main;
        }
    }

    $_dbh = $_dbh_shadow;
    # we have to return $class->dbh instead of $_dbh as
    # $_dbh_shadow may be undefined if no shadow DB is used
    # and no connection to the main DB has been established yet.
    return $class->dbh;
}

sub switch_to_main_db {
    my $class = shift;

    $_dbh = $_dbh_main;
    # We have to return $class->dbh instead of $_dbh as
    # $_dbh_main may be undefined if no connection to the main DB
    # has been established yet.
    return $class->dbh;
}

# Private methods

# Per process cleanup
sub _cleanup {
    undef $_cgi;
    undef $_user;

    # See bug 192531. If we don't clear the possibly active statement handles,
    # then when this is called from the END block, it happens _before_ the
    # destructors in Bugzilla::DB have happened.
    # See http://rt.perl.org/rt2/Ticket/Display.html?id=17450#38810
    # Without disconnecting explicitly here, noone notices, because DBI::END
    # ends up calling DBD::mysql's $drh->disconnect_all, which is a noop.
    # This code is evil, but it needs to be done, at least until SendSQL and
    # friends can be removed
    @Bugzilla::DB::SQLStateStack = ();
    undef $Bugzilla::DB::_current_sth;

    # When we support transactions, need to ->rollback here
    $_dbh_main->disconnect if $_dbh_main;
    $_dbh_shadow->disconnect if $_dbh_shadow and Param("shadowdb");
    undef $_dbh_main;
    undef $_dbh_shadow;
    undef $_dbh;
}

sub END {
    _cleanup();
}

1;

__END__

=head1 NAME

Bugzilla - Semi-persistent collection of various objects used by scripts
and modules

=head1 SYNOPSIS

  use Bugzilla;

  sub someModulesSub {
    Bugzilla->dbh->prepare(...);
    Bugzilla->template->process(...);
  }

=head1 DESCRIPTION

Several Bugzilla 'things' are used by a variety of modules and scripts. This
includes database handles, template objects, and so on.

This module is a singleton intended as a central place to store these objects.
This approach has several advantages:

=over 4

=item *

They're not global variables, so we don't have issues with them staying arround
with mod_perl

=item *

Everything is in one central place, so its easy to access, modify, and maintain

=item *

Code in modules can get access to these objects without having to have them
all passed from the caller, and the caller's caller, and....

=item *

We can reuse objects across requests using mod_perl where appropriate (eg
templates), whilst destroying those which are only valid for a single request
(such as the current user)

=back

Note that items accessible via this object are demand-loaded when requested.

For something to be added to this object, it should either be able to benefit
from persistence when run under mod_perl (such as the a C<template> object),
or should be something which is globally required by a large ammount of code
(such as the current C<user> object).

=head1 METHODS

Note that all C<Bugzilla> functionality is method based; use C<Bugzilla-E<gt>dbh>
rather than C<Bugzilla::dbh>. Nothing cares about this now, but don't rely on
that.

=over 4

=item C<template>

The current C<Template> object, to be used for output

=item C<cgi>

The current C<cgi> object. Note that modules should B<not> be using this in
general. Not all Bugzilla actions are cgi requests. Its useful as a convenience
method for those scripts/templates which are only use via CGI, though.

=item C<user>

The current C<Bugzilla::User>. C<undef> if there is no currently logged in user
or if the login code has not yet been run.

=item C<login>

Logs in a user, returning a C<Bugzilla::User> object, or C<undef> if there is
no logged in user. See L<Bugzilla::Auth|Bugzilla::Auth>, and
L<Bugzilla::User|Bugzilla::User>.

=item C<logout($option)>

Logs out the current user, which involves invalidating user sessions and
cookies. Three options are available from
L<Bugzilla::Constants|Bugzilla::Constants>: LOGOUT_CURRENT (the
default), LOGOUT_ALL or LOGOUT_KEEP_CURRENT.

=item C<logout_user($user)>

Logs out the specified user (invalidating all his sessions), taking a
Bugzilla::User instance.

=item C<logout_by_id($id)>

Logs out the user with the id specified. This is a compatibility
function to be used in callsites where there is only a userid and no
Bugzilla::User instance.

=item C<logout_request>

Essentially, causes calls to C<Bugzilla-E<gt>user> to return C<undef>. This has the
effect of logging out a user for the current request only; cookies and
database sessions are left intact.

=item C<batch>

Set to true, by calling Bugzilla->batch(1), to indicate that Bugzilla is
being called in a non-interactive manner and errors should be passed to 
die() rather than being sent to a browser and finished with an exit().
Bugzilla->batch will return the current state of this flag.

=item C<dbh>

The current database handle. See L<DBI>.

=item C<dbwritesallowed>

Determines if writes to the database are permitted. This is usually used to
determine if some general cleanup needs to occur (such as clearing the token
table)

=item C<switch_to_shadow_db>

Switch from using the main database to using the shadow database.

=item C<switch_to_main_db>

Change the database object to refer to the main database.

=back
