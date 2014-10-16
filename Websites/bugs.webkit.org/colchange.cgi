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
#                 Gervase Markham <gerv@gerv.net>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Pascal Held <paheld@gmail.com>

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::CGI;
use Bugzilla::Search::Saved;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Token;

use Storable qw(dclone);

# Maps parameters that control columns to the names of columns.
use constant COLUMN_PARAMS => {
    'useclassification'   => ['classification'],
    'usebugaliases'       => ['alias'],
    'usetargetmilestone'  => ['target_milestone'],
    'useqacontact'        => ['qa_contact', 'qa_contact_realname'],
    'usestatuswhiteboard' => ['status_whiteboard'],
};

# We only show these columns if an object of this type exists in the
# database.
use constant COLUMN_CLASSES => {
    'Bugzilla::Flag'    => 'flagtypes.name',
    'Bugzilla::Keyword' => 'keywords',
};

Bugzilla->login();

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

my $columns = dclone(Bugzilla::Search::COLUMNS);

# You can't manually select "relevance" as a column you want to see.
delete $columns->{'relevance'};

foreach my $param (keys %{ COLUMN_PARAMS() }) {
    next if Bugzilla->params->{$param};
    foreach my $column (@{ COLUMN_PARAMS->{$param} }) {
        delete $columns->{$column};
    }
}

foreach my $class (keys %{ COLUMN_CLASSES() }) {
    eval("use $class; 1;") || die $@;
    my $column = COLUMN_CLASSES->{$class};
    delete $columns->{$column} if !$class->any_exist;
}

if (!Bugzilla->user->is_timetracker) {
    foreach my $column (TIMETRACKING_FIELDS) {
        delete $columns->{$column};
    }
}

$vars->{'columns'} = $columns;

my @collist;
if (defined $cgi->param('rememberedquery')) {
    my $search;
    if (defined $cgi->param('saved_search')) {
        $search = new Bugzilla::Search::Saved($cgi->param('saved_search'));
    }

    my $token = $cgi->param('token');
    if ($search) {
        check_hash_token($token, [$search->id, $search->name]);
    }
    else {
        check_hash_token($token, ['default-list']);
    }

    my $splitheader = 0;
    if (defined $cgi->param('resetit')) {
        @collist = DEFAULT_COLUMN_LIST;
    } else {
        if (defined $cgi->param("selected_columns")) {
            @collist = grep { exists $columns->{$_} } 
                            $cgi->param("selected_columns");
        }
        if (defined $cgi->param('splitheader')) {
            $splitheader = $cgi->param('splitheader')? 1: 0;
        }
    }
    my $list = join(" ", @collist);

    if ($list) {
        # Only set the cookie if this is not a saved search.
        # Saved searches have their own column list
        if (!$cgi->param('save_columns_for_search')) {
            $cgi->send_cookie(-name => 'COLUMNLIST',
                              -value => $list,
                              -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');
        }
    }
    else {
        $cgi->remove_cookie('COLUMNLIST');
    }
    if ($splitheader) {
        $cgi->send_cookie(-name => 'SPLITHEADER',
                          -value => $splitheader,
                          -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');
    }
    else {
        $cgi->remove_cookie('SPLITHEADER');
    }

    $vars->{'message'} = "change_columns";

    if ($cgi->param('save_columns_for_search')
        && defined $search && $search->user->id == Bugzilla->user->id) 
    {
        my $params = new Bugzilla::CGI($search->url);
        $params->param('columnlist', join(",", @collist));
        $search->set_url($params->query_string());
        $search->update();
    }

    my $params = new Bugzilla::CGI($cgi->param('rememberedquery'));
    $params->param('columnlist', join(",", @collist));
    $vars->{'redirect_url'} = "buglist.cgi?".$params->query_string();


    # If we're running on Microsoft IIS, $cgi->redirect discards
    # the Set-Cookie lines. In mod_perl, $cgi->redirect with cookies
    # causes the page to be rendered as text/plain.
    # Workaround is to use the old-fashioned  redirection mechanism. 
    # See bug 214466 and bug 376044 for details.
    if ($ENV{'MOD_PERL'} 
        || $ENV{'SERVER_SOFTWARE'} =~ /Microsoft-IIS/
        || $ENV{'SERVER_SOFTWARE'} =~ /Sun ONE Web/)
    {
      print $cgi->header(-type => "text/html",
                         -refresh => "0; URL=$vars->{'redirect_url'}");
    }
    else {
      print $cgi->redirect($vars->{'redirect_url'});
      exit;
    }
    
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if (defined $cgi->param('columnlist')) {
    @collist = split(/[ ,]+/, $cgi->param('columnlist'));
} elsif (defined $cgi->cookie('COLUMNLIST')) {
    @collist = split(/ /, $cgi->cookie('COLUMNLIST'));
} else {
    @collist = DEFAULT_COLUMN_LIST;
}

$vars->{'collist'} = \@collist;
$vars->{'splitheader'} = $cgi->cookie('SPLITHEADER') ? 1 : 0;

$vars->{'buffer'} = $cgi->query_string();

my $search;
if (defined $cgi->param('query_based_on')) {
    my $searches = Bugzilla->user->queries;
    my ($search) = grep($_->name eq $cgi->param('query_based_on'), @$searches);

    if ($search) {
        $vars->{'saved_search'} = $search;
    }
}

# Generate and return the UI (HTML page) from the appropriate template.
print $cgi->header();
$template->process("list/change-columns.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
