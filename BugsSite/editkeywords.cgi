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
# The Initial Developer of the Original Code is Terry Weissman.
# Portions created by Terry Weissman are
# Copyright (C) 2000 Terry Weissman. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use strict;
use lib ".";

require "CGI.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::User;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

use vars qw($template $vars);


sub Validate ($$) {
    my ($name, $description) = @_;
    if ($name eq "") {
        ThrowUserError("keyword_blank_name");
    }
    if ($name =~ /[\s,]/) {
        ThrowUserError("keyword_invalid_name");
    }    
    if ($description eq "") {
        ThrowUserError("keyword_blank_description");
    }
}

sub ValidateKeyID {
    my $id = shift;

    $id = trim($id || 0);
    detaint_natural($id) || ThrowCodeError('invalid_keyword_id');
    return $id;
}


#
# Preliminary checks:
#

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

UserInGroup("editkeywords")
  || ThrowUserError("auth_failure", {group  => "editkeywords",
                                     action => "edit",
                                     object => "keywords"});

my $action  = trim($cgi->param('action')  || '');
$vars->{'action'} = $action;


if ($action eq "") {
    my @keywords;

    SendSQL("SELECT keyworddefs.id, keyworddefs.name, keyworddefs.description,
                    COUNT(keywords.bug_id)
             FROM keyworddefs LEFT JOIN keywords
               ON keyworddefs.id = keywords.keywordid " .
             $dbh->sql_group_by('keyworddefs.id',
                    'keyworddefs.name, keyworddefs.description') . "
             ORDER BY keyworddefs.name");

    while (MoreSQLData()) {
        my ($id, $name, $description, $bugs) = FetchSQLData();
        my $keyword = {};
        $keyword->{'id'} = $id;
        $keyword->{'name'} = $name;
        $keyword->{'description'} = $description;
        $keyword->{'bug_count'} = $bugs;
        push(@keywords, $keyword);
    }

    print Bugzilla->cgi->header();

    $vars->{'keywords'} = \@keywords;
    $template->process("admin/keywords/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}
    

if ($action eq 'add') {
    print Bugzilla->cgi->header();

    $template->process("admin/keywords/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='new' -> add keyword entered in the 'action=add' screen
#

if ($action eq 'new') {
    # Cleanups and valididy checks

    my $name = trim($cgi->param('name') || '');
    my $description  = trim($cgi->param('description')  || '');

    Validate($name, $description);
    
    SendSQL("SELECT id FROM keyworddefs WHERE name = " . SqlQuote($name));

    if (FetchOneColumn()) {
        $vars->{'name'} = $name;
        ThrowUserError("keyword_already_exists", $vars);
    }


    # Pick an unused number.  Be sure to recycle numbers that may have been
    # deleted in the past.  This code is potentially slow, but it happens
    # rarely enough, and there really aren't ever going to be that many
    # keywords anyway.

    SendSQL("SELECT id FROM keyworddefs ORDER BY id");

    my $newid = 1;

    while (MoreSQLData()) {
        my $oldid = FetchOneColumn();
        if ($oldid > $newid) {
            last;
        }
        $newid = $oldid + 1;
    }

    # Add the new keyword.
    SendSQL("INSERT INTO keyworddefs (id, name, description) VALUES ($newid, " .
            SqlQuote($name) . "," .
            SqlQuote($description) . ")");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print Bugzilla->cgi->header();

    $vars->{'name'} = $name;
    $template->process("admin/keywords/created.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

    

#
# action='edit' -> present the edit keywords from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    my $id = ValidateKeyID(scalar $cgi->param('id'));

    # get data of keyword
    SendSQL("SELECT name,description
             FROM keyworddefs
             WHERE id=$id");
    my ($name, $description) = FetchSQLData();
    if (!$name) {
        $vars->{'id'} = $id;
        ThrowCodeError("invalid_keyword_id", $vars);
    }

    SendSQL("SELECT count(*)
             FROM keywords
             WHERE keywordid = $id");
    my $bugs = '';
    $bugs = FetchOneColumn() if MoreSQLData();

    $vars->{'keyword_id'} = $id;
    $vars->{'name'} = $name;
    $vars->{'description'} = $description;
    $vars->{'bug_count'} = $bugs;

    print Bugzilla->cgi->header();

    $template->process("admin/keywords/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='update' -> update the keyword
#

if ($action eq 'update') {
    my $id = ValidateKeyID(scalar $cgi->param('id'));

    my $name  = trim($cgi->param('name') || '');
    my $description  = trim($cgi->param('description')  || '');

    Validate($name, $description);

    SendSQL("SELECT id FROM keyworddefs WHERE name = " . SqlQuote($name));

    my $tmp = FetchOneColumn();

    if ($tmp && $tmp != $id) {
        $vars->{'name'} = $name;
        ThrowUserError("keyword_already_exists", $vars);
    }

    SendSQL("UPDATE keyworddefs SET name = " . SqlQuote($name) .
            ", description = " . SqlQuote($description) .
            " WHERE id = $id");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print Bugzilla->cgi->header();

    $vars->{'name'} = $name;
    $template->process("admin/keywords/rebuild-cache.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


if ($action eq 'delete') {
    my $id = ValidateKeyID(scalar $cgi->param('id'));

    SendSQL("SELECT name FROM keyworddefs WHERE id=$id");
    my $name = FetchOneColumn();

    if (!$cgi->param('reallydelete')) {
        SendSQL("SELECT count(*)
                 FROM keywords
                 WHERE keywordid = $id");
        
        my $bugs = FetchOneColumn();
        
        if ($bugs) {
            $vars->{'bug_count'} = $bugs;
            $vars->{'keyword_id'} = $id;
            $vars->{'name'} = $name;

            print Bugzilla->cgi->header();

            $template->process("admin/keywords/confirm-delete.html.tmpl",
                               $vars)
              || ThrowTemplateError($template->error());

            exit;
        }
    }

    SendSQL("DELETE FROM keywords WHERE keywordid = $id");
    SendSQL("DELETE FROM keyworddefs WHERE id = $id");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print Bugzilla->cgi->header();

    $vars->{'name'} = $name;
    $template->process("admin/keywords/rebuild-cache.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

ThrowCodeError("action_unrecognized", $vars);
