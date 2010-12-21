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
#                 Dan Mosedale <dmose@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Myk Melez <myk@mozilla.org>
#                 Jeff Hedlund <jeff.hedlund@matrixsi.com>
#                 Frédéric Buclin <LpSolit@gmail.com>
#                 Lance Larsh <lance.larsh@oracle.com>
#                 Akamai Technologies <bugzilla-dev@akamai.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

# Implementation notes for this file:
#
# 1) the 'id' form parameter is validated early on, and if it is not a valid
# bugid an error will be reported, so it is OK for later code to simply check
# for a defined form 'id' value, and it can assume a valid bugid.
#
# 2) If the 'id' form parameter is not defined (after the initial validation),
# then we are processing multiple bugs, and @idlist will contain the ids.
#
# 3) If we are processing just the one id, then it is stored in @idlist for
# later processing.

use strict;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Mailer;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Field;
use Bugzilla::Product;
use Bugzilla::Component;
use Bugzilla::Keyword;
use Bugzilla::Flag;
use Bugzilla::Status;
use Bugzilla::Token;

use Storable qw(dclone);

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
$vars->{'use_keywords'} = 1 if Bugzilla::Keyword::keyword_count();

######################################################################
# Subroutines
######################################################################

# Used to send email when an update is done.
sub send_results {
    my ($bug_id, $vars) = @_;
    my $template = Bugzilla->template;
    if (Bugzilla->usage_mode == USAGE_MODE_EMAIL) {
         Bugzilla::BugMail::Send($bug_id, $vars->{'mailrecipients'});
    }
    else {
        $template->process("bug/process/results.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
    }
    $vars->{'header_done'} = 1;
}

# Tells us whether or not a field should be changed by process_bug.
sub should_set {
    # check_defined is used for fields where there's another field
    # whose name starts with "defined_" and then the field name--it's used
    # to know when we did things like empty a multi-select or deselect
    # a checkbox.
    my ($field, $check_defined) = @_;
    my $cgi = Bugzilla->cgi;
    if ( defined $cgi->param($field) 
         || ($check_defined && defined $cgi->param("defined_$field")) )
    {
        return 1;
    }
    return 0;
}

######################################################################
# Begin Data/Security Validation
######################################################################

# Create a list of objects for all bugs being modified in this request.
my @bug_objects;
if (defined $cgi->param('id')) {
  my $id = $cgi->param('id');
  ValidateBugID($id);

  # Store the validated, and detainted id back in the cgi data, as
  # lots of later code will need it, and will obtain it from there
  $cgi->param('id', $id);
  push(@bug_objects, new Bugzilla::Bug($id));
} else {
    my @ids;
    foreach my $i ($cgi->param()) {
        if ($i =~ /^id_([1-9][0-9]*)/) {
            my $id = $1;
            ValidateBugID($id);
            push(@ids, $id);
        }
    }
    @bug_objects = @{Bugzilla::Bug->new_from_list(\@ids)};
}

# Make sure there are bugs to process.
scalar(@bug_objects) || ThrowUserError("no_bugs_chosen", {action => 'modify'});

my $first_bug = $bug_objects[0]; # Used when we're only updating a single bug.

# Delete any parameter set to 'dontchange'.
if (defined $cgi->param('dontchange')) {
    foreach my $name ($cgi->param) {
        next if $name eq 'dontchange'; # But don't delete dontchange itself!
        # Skip ones we've already deleted (such as "defined_$name").
        next if !defined $cgi->param($name);
        if ($cgi->param($name) eq $cgi->param('dontchange')) {
            $cgi->delete($name);
            $cgi->delete("defined_$name");
        }
    }
}

# do a match on the fields if applicable

# The order of these function calls is important, as Flag::validate
# assumes User::match_field has ensured that the values
# in the requestee fields are legitimate user email addresses.
&Bugzilla::User::match_field($cgi, {
    'qa_contact'                => { 'type' => 'single' },
    'newcc'                     => { 'type' => 'multi'  },
    'masscc'                    => { 'type' => 'multi'  },
    'assigned_to'               => { 'type' => 'single' },
    '^requestee(_type)?-(\d+)$' => { 'type' => 'multi'  },
});

# Validate flags in all cases. validate() should not detect any
# reference to flags if $cgi->param('id') is undefined.
Bugzilla::Flag::validate($cgi->param('id'));

print $cgi->header() unless Bugzilla->usage_mode == USAGE_MODE_EMAIL;

# Check for a mid-air collision. Currently this only works when updating
# an individual bug.
if (defined $cgi->param('delta_ts')
    && $cgi->param('delta_ts') ne $first_bug->delta_ts)
{
    ($vars->{'operations'}) =
        Bugzilla::Bug::GetBugActivity($first_bug->id, undef,
                                      scalar $cgi->param('delta_ts'));

    $vars->{'title_tag'} = "mid_air";
    
    ThrowCodeError('undefined_field', { field => 'longdesclength' })
        if !defined $cgi->param('longdesclength');

    $vars->{'start_at'} = $cgi->param('longdesclength');
    # Always sort midair collision comments oldest to newest,
    # regardless of the user's personal preference.
    $vars->{'comments'} = Bugzilla::Bug::GetComments($first_bug->id,
                                                     "oldest_to_newest");
    $vars->{'bug'} = $first_bug;
    # The token contains the old delta_ts. We need a new one.
    $cgi->param('token', issue_hash_token([$first_bug->id, $first_bug->delta_ts]));
    
    # Warn the user about the mid-air collision and ask them what to do.
    $template->process("bug/process/midair.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# We couldn't do this check earlier as we first had to validate bug IDs
# and display the mid-air collision page if delta_ts changed.
# If we do a mass-change, we use session tokens.
my $token = $cgi->param('token');

if ($cgi->param('id')) {
    check_hash_token($token, [$first_bug->id, $first_bug->delta_ts]);
}
else {
    check_token_data($token, 'buglist_mass_change', 'query.cgi');
}

######################################################################
# End Data/Security Validation
######################################################################

$vars->{'title_tag'} = "bug_processed";

# Set up the vars for navigational <link> elements
my @bug_list;
if ($cgi->cookie("BUGLIST")) {
    @bug_list = split(/:/, $cgi->cookie("BUGLIST"));
    $vars->{'bug_list'} = \@bug_list;
}

my ($action, $next_bug);
if (defined $cgi->param('id')) {
    $action = Bugzilla->user->settings->{'post_bug_submit_action'}->{'value'};

    if ($action eq 'next_bug') {
        my $cur = lsearch(\@bug_list, $cgi->param('id'));
        if ($cur >= 0 && $cur < $#bug_list) {
            $next_bug = $bug_list[$cur + 1];
            # No need to check whether the user can see the bug or not.
            # All we want is its ID. An error will be thrown later
            # if the user cannot see it.
            $vars->{'bug'} = {bug_id => $next_bug};
        }
    }
    # Include both action = 'same_bug' and 'nothing'.
    else {
        $vars->{'bug'} = {bug_id => $cgi->param('id')};
    }
}
else {
    # param('id') is not defined when changing multiple bugs at once.
    $action = 'nothing';
}

# For each bug, we have to check if the user can edit the bug the product
# is currently in, before we allow them to change anything.
foreach my $bug (@bug_objects) {
    if (!Bugzilla->user->can_edit_product($bug->product_obj->id) ) {
        ThrowUserError("product_edit_denied",
                      { product => $bug->product });
    }
}

# For security purposes, and because lots of other checks depend on it,
# we set the product first before anything else.
my $product_change; # Used only for strict_isolation checks, right now.
if (should_set('product')) {
    foreach my $b (@bug_objects) {
        my $changed = $b->set_product(scalar $cgi->param('product'),
            { component        => scalar $cgi->param('component'),
              version          => scalar $cgi->param('version'),
              target_milestone => scalar $cgi->param('target_milestone'),
              change_confirmed => scalar $cgi->param('confirm_product_change'),
              other_bugs => \@bug_objects,
            });
        $product_change ||= $changed;
    }
}
        
# strict_isolation checks mean that we should set the groups
# immediately after changing the product.
foreach my $b (@bug_objects) {
    foreach my $group (@{$b->product_obj->groups_valid}) {
        my $gid = $group->id;
        if (should_set("bit-$gid", 1)) {
            # Check ! first to avoid having to check defined below.
            if (!$cgi->param("bit-$gid")) {
                $b->remove_group($gid);
            }
            # "== 1" is important because mass-change uses -1 to mean
            # "don't change this restriction"
            elsif ($cgi->param("bit-$gid") == 1) {
                $b->add_group($gid);
            }
        }
    }
}

if ($cgi->param('id') && (defined $cgi->param('dependson')
                          || defined $cgi->param('blocked')) )
{
    $first_bug->set_dependencies(scalar $cgi->param('dependson'),
                                 scalar $cgi->param('blocked'));
}
# Right now, you can't modify dependencies on a mass change.
else {
    $cgi->delete('dependson');
    $cgi->delete('blocked');
}

my $any_keyword_changes;
if (defined $cgi->param('keywords')) {
    foreach my $b (@bug_objects) {
        my $return =
            $b->modify_keywords(scalar $cgi->param('keywords'),
                                scalar $cgi->param('keywordaction'));
        $any_keyword_changes ||= $return;
    }
}

# Component, target_milestone, and version are in here just in case
# the 'product' field wasn't defined in the CGI. It doesn't hurt to set
# them twice.
my @set_fields = qw(op_sys rep_platform priority bug_severity
                    component target_milestone version
                    bug_file_loc status_whiteboard short_desc
                    deadline remaining_time estimated_time);
push(@set_fields, 'assigned_to') if !$cgi->param('set_default_assignee');
push(@set_fields, 'qa_contact')  if !$cgi->param('set_default_qa_contact');
my @custom_fields = Bugzilla->active_custom_fields;

my %methods = (
    bug_severity => 'set_severity',
    rep_platform => 'set_platform',
    short_desc   => 'set_summary',
    bug_file_loc => 'set_url',
);
foreach my $b (@bug_objects) {
    if (should_set('comment') || $cgi->param('work_time')) {
        # Add a comment as needed to each bug. This is done early because
        # there are lots of things that want to check if we added a comment.
        $b->add_comment(scalar($cgi->param('comment')),
            { isprivate => scalar $cgi->param('commentprivacy'),
              work_time => scalar $cgi->param('work_time') });
    }
    foreach my $field_name (@set_fields) {
        if (should_set($field_name)) {
            my $method = $methods{$field_name};
            $method ||= "set_" . $field_name;
            $b->$method($cgi->param($field_name));
        }
    }
    $b->reset_assigned_to if $cgi->param('set_default_assignee');
    $b->reset_qa_contact  if $cgi->param('set_default_qa_contact');

    # And set custom fields.
    foreach my $field (@custom_fields) {
        my $fname = $field->name;
        if (should_set($fname, 1)) {
            $b->set_custom_field($field, [$cgi->param($fname)]);
        }
    }
}

# Certain changes can only happen on individual bugs, never on mass-changes.
if (defined $cgi->param('id')) {
    # Since aliases are unique (like bug numbers), they can only be changed
    # for one bug at a time.
    if (Bugzilla->params->{"usebugaliases"} && defined $cgi->param('alias')) {
        $first_bug->set_alias($cgi->param('alias'));
    }

    # reporter_accessible and cclist_accessible--these are only set if
    # the user can change them and they appear on the page.
    if (should_set('cclist_accessible', 1)) {
        $first_bug->set_cclist_accessible($cgi->param('cclist_accessible'))
    }
    if (should_set('reporter_accessible', 1)) {
        $first_bug->set_reporter_accessible($cgi->param('reporter_accessible'))
    }
    
    # You can only mark/unmark comments as private on single bugs. If
    # you're not in the insider group, this code won't do anything.
    foreach my $field (grep(/^defined_isprivate/, $cgi->param())) {
        $field =~ /(\d+)$/;
        my $comment_id = $1;
        $first_bug->set_comment_is_private($comment_id,
                                           $cgi->param("isprivate_$comment_id"));
    }
}

# We need to check the addresses involved in a CC change before we touch 
# any bugs. What we'll do here is formulate the CC data into two arrays of
# users involved in this CC change.  Then those arrays can be used later 
# on for the actual change.
my (@cc_add, @cc_remove);
if (defined $cgi->param('newcc')
    || defined $cgi->param('addselfcc')
    || defined $cgi->param('removecc')
    || defined $cgi->param('masscc')) {
        
    # If masscc is defined, then we came from buglist and need to either add or
    # remove cc's... otherwise, we came from bugform and may need to do both.
    my ($cc_add, $cc_remove) = "";
    if (defined $cgi->param('masscc')) {
        if ($cgi->param('ccaction') eq 'add') {
            $cc_add = join(' ',$cgi->param('masscc'));
        } elsif ($cgi->param('ccaction') eq 'remove') {
            $cc_remove = join(' ',$cgi->param('masscc'));
        }
    } else {
        $cc_add = join(' ',$cgi->param('newcc'));
        # We came from bug_form which uses a select box to determine what cc's
        # need to be removed...
        if (defined $cgi->param('removecc') && $cgi->param('cc')) {
            $cc_remove = join (",", $cgi->param('cc'));
        }
    }

    push(@cc_add, split(/[\s,]+/, $cc_add)) if $cc_add;
    push(@cc_add, Bugzilla->user) if $cgi->param('addselfcc');

    push(@cc_remove, split(/[\s,]+/, $cc_remove)) if $cc_remove;
}

foreach my $b (@bug_objects) {
    $b->remove_cc($_) foreach @cc_remove;
    $b->add_cc($_) foreach @cc_add;
    # Theoretically you could move a product without ever specifying
    # a new assignee or qa_contact, or adding/removing any CCs. So,
    # we have to check that the current assignee, qa, and CCs are still
    # valid if we've switched products, under strict_isolation. We can only
    # do that here. There ought to be some better way to do this,
    # architecturally, but I haven't come up with it.
    if ($product_change) {
        $b->_check_strict_isolation();
    }
}

my $move_action = $cgi->param('action') || '';
if ($move_action eq Bugzilla->params->{'move-button-text'}) {
    Bugzilla->params->{'move-enabled'} || ThrowUserError("move_bugs_disabled");

    $user->is_mover || ThrowUserError("auth_failure", {action => 'move',
                                                       object => 'bugs'});

    $dbh->bz_start_transaction();

    # First update all moved bugs.
    foreach my $bug (@bug_objects) {
        $bug->add_comment('', { type => CMT_MOVED_TO, extra_data => $user->login });
    }
    # Don't export the new status and resolution. We want the current ones.
    local $Storable::forgive_me = 1;
    my $bugs = dclone(\@bug_objects);

    my $new_status = Bugzilla->params->{'duplicate_or_move_bug_status'};
    foreach my $bug (@bug_objects) {
        $bug->set_status($new_status, {resolution => 'MOVED', moving => 1});
    }
    $_->update() foreach @bug_objects;
    $dbh->bz_commit_transaction();

    # Now send emails.
    foreach my $bug (@bug_objects) {
        $vars->{'mailrecipients'} = { 'changer' => $user->login };
        $vars->{'id'} = $bug->id;
        $vars->{'type'} = "move";
        send_results($bug->id, $vars);
    }
    # Prepare and send all data about these bugs to the new database
    my $to = Bugzilla->params->{'move-to-address'};
    $to =~ s/@/\@/;
    my $from = Bugzilla->params->{'moved-from-address'};
    $from =~ s/@/\@/;
    my $msg = "To: $to\n";
    $msg .= "From: Bugzilla <" . $from . ">\n";
    $msg .= "Subject: Moving bug(s) " . join(', ', map($_->id, @bug_objects))
            . "\n\n";

    my @fieldlist = (Bugzilla::Bug->fields, 'group', 'long_desc',
                     'attachment', 'attachmentdata');
    my %displayfields;
    foreach (@fieldlist) {
        $displayfields{$_} = 1;
    }

    $template->process("bug/show.xml.tmpl", { bugs => $bugs,
                                              displayfields => \%displayfields,
                                            }, \$msg)
      || ThrowTemplateError($template->error());

    $msg .= "\n";
    MessageToMTA($msg);

    # End the response page.
    unless (Bugzilla->usage_mode == USAGE_MODE_EMAIL) {
        $template->process("bug/navigate.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
        $template->process("global/footer.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
    }
    exit;
}


# You cannot mark bugs as duplicates when changing several bugs at once
# (because currently there is no way to check for duplicate loops in that
# situation).
if (!$cgi->param('id') && $cgi->param('dup_id')) {
    ThrowUserError('dupe_not_allowed');
}

# Set the status, resolution, and dupe_of (if needed). This has to be done
# down here, because the validity of status changes depends on other fields,
# such as Target Milestone.
foreach my $b (@bug_objects) {
    if (should_set('bug_status')) {
        $b->set_status(
            scalar $cgi->param('bug_status'),
            {resolution =>  scalar $cgi->param('resolution'),
                dupe_of => scalar $cgi->param('dup_id')}
            );
    }
    elsif (should_set('resolution')) {
       $b->set_resolution(scalar $cgi->param('resolution'), 
                          {dupe_of => scalar $cgi->param('dup_id')});
    }
    elsif (should_set('dup_id')) {
        $b->set_dup_id(scalar $cgi->param('dup_id'));
    }
}

##############################
# Do Actual Database Updates #
##############################
foreach my $bug (@bug_objects) {
    $dbh->bz_start_transaction();

    my $timestamp = $dbh->selectrow_array(q{SELECT NOW()});
    my $changes = $bug->update($timestamp);

    my %notify_deps;
    if ($changes->{'bug_status'}) {
        my ($old_status, $new_status) = @{ $changes->{'bug_status'} };
        
        # If this bug has changed from opened to closed or vice-versa,
        # then all of the bugs we block need to be notified.
        if (is_open_state($old_status) ne is_open_state($new_status)) {
            $notify_deps{$_} = 1 foreach (@{$bug->blocked});
        }
        
        # We may have zeroed the remaining time, if we moved into a closed
        # status, so we should inform the user about that.
        if (!is_open_state($new_status) && $changes->{'remaining_time'}) {
            $vars->{'message'} = "remaining_time_zeroed"
              if Bugzilla->user->in_group(Bugzilla->params->{'timetrackinggroup'});
        }
    }

    # To get a list of all changed dependencies, convert the "changes" arrays
    # into a long string, then collapse that string into unique numbers in
    # a hash.
    my $all_changed_deps = join(', ', @{ $changes->{'dependson'} || [] });
    $all_changed_deps = join(', ', @{ $changes->{'blocked'} || [] },
                                   $all_changed_deps);
    my %changed_deps = map { $_ => 1 } split(', ', $all_changed_deps);
    # When clearning one field (say, blocks) and filling in the other
    # (say, dependson), an empty string can get into the hash and cause
    # an error later.
    delete $changed_deps{''};

    # $msgs will store emails which have to be sent to voters, if any.
    my $msgs;
    if ($changes->{'product'}) {
        # If some votes have been removed, RemoveVotes() returns
        # a list of messages to send to voters.
        # We delay the sending of these messages till tables are unlocked.
        $msgs = RemoveVotes($bug->id, 0, 'votes_bug_moved');
        CheckIfVotedConfirmed($bug->id, Bugzilla->user->id);
    }

    # Set and update flags.
    Bugzilla::Flag->process($bug, undef, $timestamp, $vars);

    $dbh->bz_commit_transaction();

    ###############
    # Send Emails #
    ###############

    # Now is a good time to send email to voters.
    foreach my $msg (@$msgs) {
        MessageToMTA($msg);
    }

    my $old_qa  = $changes->{'qa_contact'}  ? $changes->{'qa_contact'}->[0] : '';
    my $old_own = $changes->{'assigned_to'} ? $changes->{'assigned_to'}->[0] : '';
    my $old_cc  = $changes->{cc}            ? $changes->{cc}->[0] : '';
    $vars->{'mailrecipients'} = {
        cc        => [split(/[\s,]+/, $old_cc)],
        owner     => $old_own,
        qacontact => $old_qa,
        changer   => Bugzilla->user->login };

    $vars->{'id'} = $bug->id;
    $vars->{'type'} = "bug";
    
    # Let the user know the bug was changed and who did and didn't
    # receive email about the change.
    send_results($bug->id, $vars);
 
    # If the bug was marked as a duplicate, we need to notify users on the
    # other bug of any changes to that bug.
    my $new_dup_id = $changes->{'dup_id'} ? $changes->{'dup_id'}->[1] : undef;
    if ($new_dup_id) {
        $vars->{'mailrecipients'} = { 'changer' => Bugzilla->user->login }; 

        $vars->{'id'} = $new_dup_id;
        $vars->{'type'} = "dupe";
        
        # Let the user know a duplication notation was added to the 
        # original bug.
        send_results($new_dup_id, $vars);
    }

    my %all_dep_changes = (%notify_deps, %changed_deps);
    foreach my $id (sort { $a <=> $b } (keys %all_dep_changes)) {
        $vars->{'mailrecipients'} = { 'changer' => Bugzilla->user->login };
        $vars->{'id'} = $id;
        $vars->{'type'} = "dep";

        # Let the user (if he is able to see the bug) know we checked to
        # see if we should email notice of this change to users with a 
        # relationship to the dependent bug and who did and didn't 
        # receive email about it.
        send_results($id, $vars);
    }
}

# Determine if Patch Viewer is installed, for Diff link
# (NB: Duplicate code with show_bug.cgi.)
eval {
    require PatchReader;
    $vars->{'patchviewerinstalled'} = 1;
};

if (Bugzilla->usage_mode == USAGE_MODE_EMAIL) {
    # Do nothing.
}
elsif ($action eq 'next_bug') {
    if ($next_bug) {
        if (detaint_natural($next_bug) && Bugzilla->user->can_see_bug($next_bug)) {
            my $bug = new Bugzilla::Bug($next_bug);
            ThrowCodeError("bug_error", { bug => $bug }) if $bug->error;

            $vars->{'bugs'} = [$bug];
            $vars->{'nextbug'} = $bug->bug_id;

            $template->process("bug/show.html.tmpl", $vars)
              || ThrowTemplateError($template->error());

            exit;
        }
    }
} elsif ($action eq 'same_bug') {
    if (Bugzilla->user->can_see_bug($cgi->param('id'))) {
        my $bug = new Bugzilla::Bug($cgi->param('id'));
        ThrowCodeError("bug_error", { bug => $bug }) if $bug->error;

        $vars->{'bugs'} = [$bug];

        $template->process("bug/show.html.tmpl", $vars)
          || ThrowTemplateError($template->error());

        exit;
    }
} elsif ($action ne 'nothing') {
    ThrowCodeError("invalid_post_bug_submit_action");
}

# End the response page.
unless (Bugzilla->usage_mode == USAGE_MODE_EMAIL) {
    $template->process("bug/navigate.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    $template->process("global/footer.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
}

1;
