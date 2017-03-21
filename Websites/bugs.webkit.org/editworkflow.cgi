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
use Bugzilla::Token;
use Bugzilla::Status;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('admin')
  || ThrowUserError('auth_failure', {group  => 'admin',
                                     action => 'modify',
                                     object => 'workflow'});

my $action = $cgi->param('action') || 'edit';
my $token = $cgi->param('token');

sub get_workflow {
    my $dbh = Bugzilla->dbh;
    my $workflow = $dbh->selectall_arrayref('SELECT old_status, new_status, require_comment
                                             FROM status_workflow');
    my %workflow;
    foreach my $row (@$workflow) {
        my ($old, $new, $type) = @$row;
        $workflow{$old || 0}{$new} = $type;
    }
    return \%workflow;
}

sub load_template {
    my ($filename, $message) = @_;
    my $template = Bugzilla->template;
    my $vars = {};

    $vars->{'statuses'} = [Bugzilla::Status->get_all];
    $vars->{'workflow'} = get_workflow();
    $vars->{'token'} = issue_session_token("workflow_$filename");
    $vars->{'message'} = $message;

    $template->process("admin/workflow/$filename.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'edit') {
    load_template('edit');
}
elsif ($action eq 'update') {
    check_token_data($token, 'workflow_edit');
    my $statuses = [Bugzilla::Status->get_all];
    my $workflow = get_workflow();

    my $sth_insert = $dbh->prepare('INSERT INTO status_workflow (old_status, new_status)
                                    VALUES (?, ?)');
    my $sth_delete = $dbh->prepare('DELETE FROM status_workflow
                                    WHERE old_status = ? AND new_status = ?');
    my $sth_delnul = $dbh->prepare('DELETE FROM status_workflow
                                    WHERE old_status IS NULL AND new_status = ?');

    # Part 1: Initial bug statuses.
    foreach my $new (@$statuses) {
        if ($cgi->param('w_0_' . $new->id)) {
            $sth_insert->execute(undef, $new->id)
              unless defined $workflow->{0}->{$new->id};
        }
        else {
            $sth_delnul->execute($new->id);
        }
    }

    # Part 2: Bug status changes.
    foreach my $old (@$statuses) {
        foreach my $new (@$statuses) {
            next if $old->id == $new->id;

            # All transitions to 'duplicate_or_move_bug_status' must be valid.
            if ($cgi->param('w_' . $old->id . '_' . $new->id)
                || ($new->name eq Bugzilla->params->{'duplicate_or_move_bug_status'}))
            {
                $sth_insert->execute($old->id, $new->id)
                  unless defined $workflow->{$old->id}->{$new->id};
            }
            else {
                $sth_delete->execute($old->id, $new->id);
            }
        }
    }
    delete_token($token);
    load_template('edit', 'workflow_updated');
}
elsif ($action eq 'edit_comment') {
    load_template('comment');
}
elsif ($action eq 'update_comment') {
    check_token_data($token, 'workflow_comment');
    my $workflow = get_workflow();

    my $sth_update = $dbh->prepare('UPDATE status_workflow SET require_comment = ?
                                    WHERE old_status = ? AND new_status = ?');
    my $sth_updnul = $dbh->prepare('UPDATE status_workflow SET require_comment = ?
                                    WHERE old_status IS NULL AND new_status = ?');

    foreach my $old (keys %$workflow) {
        # Hashes cannot have undef as a key, so we use 0. But the DB
        # must store undef, for referential integrity.
        my $old_id_for_db = $old || undef;
        foreach my $new (keys %{$workflow->{$old}}) {
            my $comment_required = $cgi->param("c_${old}_$new") ? 1 : 0;
            next if ($workflow->{$old}->{$new} == $comment_required);
            if ($old_id_for_db) {
                $sth_update->execute($comment_required, $old_id_for_db, $new);
            }
            else {
                $sth_updnul->execute($comment_required, $new);
            }
        }
    }
    delete_token($token);
    load_template('comment', 'workflow_updated');
}
else {
    ThrowUserError('unknown_action', {action => $action});
}
