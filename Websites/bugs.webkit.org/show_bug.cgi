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
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Bug;

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

my $user = Bugzilla->login();

my $format = $template->get_format("bug/show", scalar $cgi->param('format'),
                                   scalar $cgi->param('ctype'));

# Editable, 'single' HTML bugs are treated slightly specially in a few places
my $single = !$format->{format} && $format->{extension} eq 'html';

# If we don't have an ID, _AND_ we're only doing a single bug, then prompt
if (!$cgi->param('id') && $single) {
    print $cgi->header();
    $template->process("bug/choose.html.tmpl", $vars) ||
      ThrowTemplateError($template->error());
    exit;
}

my (@bugs, @illegal_bugs);
my %marks;

# If the user isn't logged in, we use data from the shadow DB. If they plan
# to edit the bug(s), they will have to log in first, meaning that the data
# will be reloaded anyway, from the main DB.
Bugzilla->switch_to_shadow_db unless $user->id;

if ($single) {
    my $id = $cgi->param('id');
    push @bugs, Bugzilla::Bug->check({ id => $id, cache => 1 });
    if (defined $cgi->param('mark')) {
        foreach my $range (split ',', $cgi->param('mark')) {
            if ($range =~ /^(\d+)-(\d+)$/) {
               foreach my $i ($1..$2) {
                   $marks{$i} = 1;
               }
            } elsif ($range =~ /^(\d+)$/) {
               $marks{$1} = 1;
            }
        }
    }
} else {
    foreach my $id ($cgi->param('id')) {
        # Be kind enough and accept URLs of the form: id=1,2,3.
        my @ids = split(/,/, $id);
        my @check_bugs;

        foreach my $bug_id (@ids) {
            next unless $bug_id;
            my $bug = new Bugzilla::Bug({ id => $bug_id, cache => 1 });
            if (!$bug->{error}) {
                push(@check_bugs, $bug);
            }
            else {
                push(@illegal_bugs, { bug_id => trim($bug_id), error => $bug->{error} });
            }
        }

        $user->visible_bugs(\@check_bugs);

        foreach my $bug (@check_bugs) {
            if ($user->can_see_bug($bug->id)) {
                push(@bugs, $bug);
            }
            else {
                my $error = 'NotPermitted'; # Trick to make 012throwables.t happy.
                push(@illegal_bugs, { bug_id => $bug->id, error => $error });
            }
        }
    }
}

Bugzilla::Bug->preload(\@bugs);

$vars->{'bugs'} = [@bugs, @illegal_bugs];
$vars->{'marks'} = \%marks;

my @bugids = map {$_->bug_id} grep {!$_->error} @bugs;
$vars->{'bugids'} = join(", ", @bugids);

# Work out which fields we are displaying (currently XML only.)
# If no explicit list is defined, we show all fields. We then exclude any
# on the exclusion list. This is so you can say e.g. "Everything except 
# attachments" without listing almost all the fields.
my @fieldlist = (Bugzilla::Bug->fields, 'flag', 'group', 'long_desc',
                 'attachment', 'attachmentdata', 'token');
my %displayfields;

if ($cgi->param("field")) {
    @fieldlist = $cgi->param("field");
}

unless ($user->is_timetracker) {
    @fieldlist = grep($_ !~ /_time$/, @fieldlist);
}

foreach (@fieldlist) {
    $displayfields{$_} = 1;
}

foreach ($cgi->param("excludefield")) {
    $displayfields{$_} = undef;    
}

$vars->{'displayfields'} = \%displayfields;

print $cgi->header($format->{'ctype'});

$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());
