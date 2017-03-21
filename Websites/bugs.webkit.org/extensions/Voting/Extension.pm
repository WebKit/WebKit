# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Voting;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;
use Bugzilla::Mailer;
use Bugzilla::User;
use Bugzilla::Util qw(detaint_natural);
use Bugzilla::Token;

use List::Util qw(min sum);

use constant VERSION => BUGZILLA_VERSION;
use constant DEFAULT_VOTES_PER_BUG => 1;
# These came from Bugzilla itself, so they maintain the old numbers
# they had before.
use constant CMT_POPULAR_VOTES => 3;
use constant REL_VOTER => 4;

################
# Installation #
################

BEGIN {
    *Bugzilla::Bug::votes = \&votes;
}

sub votes {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    return $self->{votes} if exists $self->{votes};

    $self->{votes} = $dbh->selectrow_array('SELECT votes FROM bugs WHERE bug_id = ?',
                                           undef, $self->id);
    return $self->{votes};
}

sub db_schema_abstract_schema {
    my ($self, $args) = @_;
    $args->{'schema'}->{'votes'} = {
        FIELDS => [
            who        => {TYPE => 'INT3', NOTNULL => 1,
                           REFERENCES => {TABLE  => 'profiles',
                                          COLUMN => 'userid',
                                          DELETE => 'CASCADE'}},
            bug_id     => {TYPE => 'INT3', NOTNULL => 1,
                           REFERENCES  => {TABLE  =>  'bugs',
                                           COLUMN =>  'bug_id',
                                           DELETE => 'CASCADE'}},
            vote_count => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            votes_who_idx    => ['who'],
            votes_bug_id_idx => ['bug_id'],
        ],
    };
}

sub install_update_db {
    my $dbh = Bugzilla->dbh;
    # Note that before Bugzilla 4.0, voting was a built-in part of Bugzilla,
    # so updates to the columns for old versions of Bugzilla happen in
    # Bugzilla::Install::DB, and can't safely be moved to this extension.

    my $field = new Bugzilla::Field({ name => 'votes' });
    if (!$field) {
        Bugzilla::Field->create(
            { name => 'votes', description => 'Votes', buglist => 1 });
    }

    $dbh->bz_add_column('products', 'votesperuser',
        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
    $dbh->bz_add_column('products', 'maxvotesperbug', 
        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => DEFAULT_VOTES_PER_BUG});
    $dbh->bz_add_column('products', 'votestoconfirm',
        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});

    $dbh->bz_add_column('bugs', 'votes',
        {TYPE => 'INT3', NOTNULL => 1, DEFAULT => 0});
    $dbh->bz_add_index('bugs', 'bugs_votes_idx', ['votes']);

    # maxvotesperbug used to default to 10,000, which isn't very sensible.
    my $per_bug = $dbh->bz_column_info('products', 'maxvotesperbug');
    if ($per_bug->{DEFAULT} != DEFAULT_VOTES_PER_BUG) {
        $dbh->bz_alter_column('products', 'maxvotesperbug',
            {TYPE => 'INT2', NOTNULL => 1, DEFAULT => DEFAULT_VOTES_PER_BUG});
    }
}

###########
# Objects #
###########

sub object_columns {
    my ($self, $args) = @_;
    my ($class, $columns) = @$args{qw(class columns)};
    if ($class->isa('Bugzilla::Bug')) {
        push(@$columns, 'votes');
    }
    elsif ($class->isa('Bugzilla::Product')) {
        push(@$columns, qw(votesperuser maxvotesperbug votestoconfirm));
    }
}

sub bug_fields {
    my ($self, $args) = @_;
    my $fields = $args->{fields};
    push(@$fields, 'votes');
}

sub object_update_columns {
    my ($self, $args) = @_;
    my ($object, $columns) = @$args{qw(object columns)};
    if ($object->isa('Bugzilla::Product')) {
        push(@$columns, qw(votesperuser maxvotesperbug votestoconfirm));
    }
}

sub object_validators {
    my ($self, $args) = @_;
    my ($class, $validators) = @$args{qw(class validators)};
    if ($class->isa('Bugzilla::Product')) {
        $validators->{'votesperuser'}   = \&_check_votesperuser;
        $validators->{'maxvotesperbug'} = \&_check_maxvotesperbug;
        $validators->{'votestoconfirm'} = \&_check_votestoconfirm;
    }
}

sub object_before_create {
    my ($self, $args) = @_;
    my ($class, $params) = @$args{qw(class params)};
    if ($class->isa('Bugzilla::Bug')) {
        # Don't ever allow people to directly specify "votes" into the bugs
        # table.
        delete $params->{votes};
    }
    elsif ($class->isa('Bugzilla::Product')) {
        my $input = Bugzilla->input_params;
        $params->{votesperuser}   = $input->{'votesperuser'};
        $params->{maxvotesperbug} = $input->{'maxvotesperbug'};
        $params->{votestoconfirm} = $input->{'votestoconfirm'};
    }
}

sub object_end_of_set_all {
    my ($self, $args) = @_;
    my ($object) = $args->{object};
    if ($object->isa('Bugzilla::Product')) {
        my $input = Bugzilla->input_params;
        $object->set('votesperuser',   $input->{'votesperuser'});
        $object->set('maxvotesperbug', $input->{'maxvotesperbug'});
        $object->set('votestoconfirm', $input->{'votestoconfirm'});
    }
}

sub object_end_of_update {
    my ($self, $args) = @_;
    my ($object, $changes) = @$args{qw(object changes)};
    if ( $object->isa('Bugzilla::Product')
         and ($changes->{maxvotesperbug} or $changes->{votesperuser} 
              or $changes->{votestoconfirm}) ) 
    {
        _modify_bug_votes($object, $changes);
    }
}

sub bug_end_of_update {
    my ($self, $args) = @_;
    my ($bug, $changes) = @$args{qw(bug changes)};

    if ($changes->{'product'}) {
        my @msgs;
        # If some votes have been removed, RemoveVotes() returns
        # a list of messages to send to voters.
        @msgs = _remove_votes($bug->id, 0, 'votes_bug_moved');
        _confirm_if_vote_confirmed($bug);

        foreach my $msg (@msgs) {
            MessageToMTA($msg);
        }
    }
}

#############
# Templates #
#############

sub template_before_create {
    my ($self, $args) = @_;
    my $config = $args->{config};
    my $constants = $config->{CONSTANTS};
    $constants->{REL_VOTER} = REL_VOTER;
    $constants->{CMT_POPULAR_VOTES} = CMT_POPULAR_VOTES;
    $constants->{DEFAULT_VOTES_PER_BUG} = DEFAULT_VOTES_PER_BUG;
}


sub template_before_process {
    my ($self, $args) = @_;
    my ($vars, $file) = @$args{qw(vars file)};
    if ($file eq 'admin/users/confirm-delete.html.tmpl') {
        my $who = $vars->{otheruser};
        my $votes = Bugzilla->dbh->selectrow_array(
            'SELECT COUNT(*) FROM votes WHERE who = ?', undef, $who->id);
        if ($votes) {
            $vars->{other_safe} = 1;
            $vars->{votes} = $votes;
        }
    }
}

###########
# Bugmail #
###########

sub bugmail_recipients {
    my ($self, $args) = @_;
    my ($bug, $recipients) = @$args{qw(bug recipients)};
    my $dbh = Bugzilla->dbh;

    my $voters = $dbh->selectcol_arrayref(
        "SELECT who FROM votes WHERE bug_id = ?", undef, $bug->id);
    $recipients->{$_}->{+REL_VOTER} = 1 foreach (@$voters);
}

sub bugmail_relationships {
    my ($self, $args) = @_;
    my $relationships = $args->{relationships};
    $relationships->{+REL_VOTER} = 'Voter';
}

###############
# Sanitycheck #
###############

sub sanitycheck_check {
    my ($self, $args) = @_;
    my $status = $args->{status};

    # Vote Cache
    $status->('voting_count_start');
    my $dbh = Bugzilla->dbh;
    my %cached_counts = @{ $dbh->selectcol_arrayref(
        'SELECT bug_id, votes FROM bugs', {Columns=>[1,2]}) };

    my %real_counts = @{ $dbh->selectcol_arrayref(
        'SELECT bug_id, SUM(vote_count) FROM votes '
        . $dbh->sql_group_by('bug_id'), {Columns=>[1,2]}) };

    my $needs_rebuild;
    foreach my $id (keys %cached_counts) {
        my $cached_count = $cached_counts{$id};
        my $real_count = $real_counts{$id} || 0;
        if ($cached_count < 0) {
            $status->('voting_count_alert', { id => $id }, 'alert');
        }
        elsif ($cached_count != $real_count) {
            $status->('voting_cache_alert', { id => $id }, 'alert');
            $needs_rebuild = 1;
        }
    }

    $status->('voting_cache_rebuild_fix') if $needs_rebuild;
}

sub sanitycheck_repair {
    my ($self, $args) = @_;
    my $status = $args->{status};
    my $input = Bugzilla->input_params;
    my $dbh = Bugzilla->dbh;

    return if !$input->{rebuild_vote_cache};

    $status->('voting_cache_rebuild_start');
    $dbh->bz_start_transaction();
    $dbh->do('UPDATE bugs SET votes = 0');

    my $sth = $dbh->prepare(
        'SELECT bug_id, SUM(vote_count) FROM votes '
        . $dbh->sql_group_by('bug_id'));
    $sth->execute();

    my $sth_update = $dbh->prepare(
        'UPDATE bugs SET votes = ? WHERE bug_id = ?');
    while (my ($id, $count) = $sth->fetchrow_array) {
        $sth_update->execute($count, $id);
    }
    $dbh->bz_commit_transaction();
    $status->('voting_cache_rebuild_end');
}


##############
# Validators #
##############

sub _check_votesperuser {
    return _check_votes(0, @_);
}

sub _check_maxvotesperbug {
    return _check_votes(DEFAULT_VOTES_PER_BUG, @_);
}

sub _check_votestoconfirm {
    return _check_votes(0, @_);
}

# This subroutine is only used internally by other _check_votes_* validators.
sub _check_votes {
    my ($default, $invocant, $votes, $field) = @_;

    detaint_natural($votes) if defined $votes;
    # On product creation, if the number of votes is not a valid integer,
    # we silently fall back to the given default value.
    # If the product already exists and the change is illegal, we complain.
    if (!defined $votes) {
        if (ref $invocant) {
            ThrowUserError('voting_product_illegal_votes',
                           { field => $field, votes => $_[2] });
        }
        else {
            $votes = $default;
        }
    }
    return $votes;
}

#########
# Pages #
#########

sub page_before_template {
    my ($self, $args) = @_;
    my $page = $args->{page_id};
    my $vars = $args->{vars};

    if ($page =~ m{^voting/bug\.}) {
        _page_bug($vars);
    }
    elsif ($page =~ m{^voting/user\.}) {
        _page_user($vars);
    }
}

sub _page_bug {
    my ($vars) = @_;
    my $dbh = Bugzilla->dbh;
    my $input = Bugzilla->input_params;

    my $bug_id = $input->{bug_id};
    my $bug = Bugzilla::Bug->check($bug_id);

    $vars->{'bug'} = $bug;
    $vars->{'users'} =
        $dbh->selectall_arrayref('SELECT profiles.login_name,
                                         profiles.userid AS id,
                                         votes.vote_count
                                    FROM votes
                              INNER JOIN profiles
                                      ON profiles.userid = votes.who
                                   WHERE votes.bug_id = ?',
                                  {Slice=>{}}, $bug->id);
}

sub _page_user {
    my ($vars) = @_;
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->user;
    my $input = Bugzilla->input_params;

    my $action = $input->{action};
    if ($action and $action eq 'vote') {
        _update_votes($vars);
    }

    # If a bug_id is given, and we're editing, we'll add it to the votes list.
    
    my $bug_id = $input->{bug_id};
    my $bug = Bugzilla::Bug->check({ id => $bug_id, cache => 1 }) if $bug_id;
    my $who_id = $input->{user_id} || $user->id;

    # Logged-out users must specify a user_id.
    Bugzilla->login(LOGIN_REQUIRED) if !$who_id;

    my $who = Bugzilla::User->check({ id => $who_id });

    my $canedit = $user->id == $who->id;

    $dbh->bz_start_transaction();

    if ($canedit && $bug) {
        # Make sure there is an entry for this bug
        # in the vote table, just so that things display right.
        my $has_votes = $dbh->selectrow_array('SELECT vote_count FROM votes 
                                               WHERE bug_id = ? AND who = ?',
                                               undef, ($bug->id, $who->id));
        if (!$has_votes) {
            $dbh->do('INSERT INTO votes (who, bug_id, vote_count) 
                      VALUES (?, ?, 0)', undef, ($who->id, $bug->id));
        }
    }

    my (@products, @all_bug_ids);
    # Read the votes data for this user for each product.
    foreach my $product (@{ $user->get_selectable_products }) {
        next unless ($product->{votesperuser} > 0);

        my $vote_list =
          $dbh->selectall_arrayref('SELECT votes.bug_id, votes.vote_count
                                      FROM votes
                                INNER JOIN bugs
                                        ON votes.bug_id = bugs.bug_id
                                     WHERE votes.who = ?
                                       AND bugs.product_id = ?',
                                     undef, ($who->id, $product->id));

        my %votes = map { $_->[0] => $_->[1] } @$vote_list;
        my @bug_ids = sort keys %votes;
        # Exclude bugs that the user can no longer see.
        @bug_ids = @{ $user->visible_bugs(\@bug_ids) };
        next unless scalar @bug_ids;

        push(@all_bug_ids, @bug_ids);
        my @bugs = @{ Bugzilla::Bug->new_from_list(\@bug_ids) };
        $_->{count} = $votes{$_->id} foreach @bugs;
        # We include votes from bugs that the user can no longer see.
        my $total = sum(values %votes) || 0;

        my $onevoteonly = 0;
        $onevoteonly = 1 if (min($product->{votesperuser},
                                 $product->{maxvotesperbug}) == 1);

        push(@products, { name => $product->name,
                          bugs => \@bugs,
                          bug_ids => \@bug_ids,
                          onevoteonly => $onevoteonly,
                          total => $total,
                          maxvotes => $product->{votesperuser},
                          maxperbug => $product->{maxvotesperbug} });
    }

    if ($canedit && $bug) {
        $dbh->do('DELETE FROM votes WHERE vote_count = 0 AND who = ?',
                 undef, $who->id);
    }
    $dbh->bz_commit_transaction();

    $vars->{'canedit'} = $canedit;
    $vars->{'voting_user'} = { "login" => $who->name };
    $vars->{'products'} = \@products;
    $vars->{'this_bug'} = $bug;
    $vars->{'all_bug_ids'} = \@all_bug_ids;
}

sub _update_votes {
    my ($vars) = @_;

    ############################################################################
    # Begin Data/Security Validation
    ############################################################################

    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    my $input = Bugzilla->input_params;

    # Build a list of bug IDs for which votes have been submitted.  Votes
    # are submitted in form fields in which the field names are the bug 
    # IDs and the field values are the number of votes.

    my @buglist = grep {/^\d+$/} keys %$input;
    my (%bugs, %votes);

    # If no bugs are in the buglist, let's make sure the user gets notified
    # that their votes will get nuked if they continue.
    if (scalar(@buglist) == 0) {
        if (!defined $cgi->param('delete_all_votes')) {
            print $cgi->header();
            $template->process("voting/delete-all.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
            exit;
        }
        elsif ($cgi->param('delete_all_votes') == 0) {
            print $cgi->redirect("page.cgi?id=voting/user.html");
            exit;
        }
    }
    else {
        $user->visible_bugs(\@buglist);
        my $bugs_obj = Bugzilla::Bug->new_from_list(\@buglist);
        $bugs{$_->id} = $_ foreach @$bugs_obj;
    }

    # Call check_is_visible() on each bug to make sure it is an existing bug
    # that the user is authorized to access, and make sure the number of votes
    # submitted is also an integer.
    foreach my $id (@buglist) {
        my $bug = $bugs{$id}
          or ThrowUserError('bug_id_does_not_exist', { bug_id => $id });
        $bug->check_is_visible;
        $id = $bug->id;
        $votes{$id} = $input->{$id};
        detaint_natural($votes{$id})
          || ThrowUserError("voting_must_be_nonnegative");
    }

    my $token = $cgi->param('token');
    check_hash_token($token, ['vote']);

    ############################################################################
    # End Data/Security Validation
    ############################################################################
    my $who = $user->id;

    # If the user is voting for bugs, make sure they aren't overstuffing
    # the ballot box.
    if (scalar @buglist) {
        my (%prodcount, %products);
        foreach my $bug_id (keys %bugs) {
            my $bug = $bugs{$bug_id};
            my $prod = $bug->product;
            $products{$prod} ||= $bug->product_obj;
            $prodcount{$prod} ||= 0;
            $prodcount{$prod} += $votes{$bug_id};

            # Make sure we haven't broken the votes-per-bug limit
            ($votes{$bug_id} <= $products{$prod}->{maxvotesperbug})
              || ThrowUserError("voting_too_many_votes_for_bug",
                                {max => $products{$prod}->{maxvotesperbug},
                                 product => $prod,
                                 votes => $votes{$bug_id}});
        }

        # Make sure we haven't broken the votes-per-product limit
        foreach my $prod (keys(%prodcount)) {
            ($prodcount{$prod} <= $products{$prod}->{votesperuser})
              || ThrowUserError("voting_too_many_votes_for_product",
                                {max => $products{$prod}->{votesperuser},
                                 product => $prod,
                                 votes => $prodcount{$prod}});
        }
    }

    # Update the user's votes in the database.
    $dbh->bz_start_transaction();

    my $old_list = $dbh->selectall_arrayref('SELECT bug_id, vote_count FROM votes
                                             WHERE who = ?', undef, $who);

    my %old_votes = map { $_->[0] => $_->[1] } @$old_list;

    my $sth_insertVotes = $dbh->prepare('INSERT INTO votes (who, bug_id, vote_count)
                                         VALUES (?, ?, ?)');
    my $sth_updateVotes = $dbh->prepare('UPDATE votes SET vote_count = ?
                                         WHERE bug_id = ? AND who = ?');

    my %affected = map { $_ => 1 } (@buglist, keys %old_votes);
    my @deleted_votes;

    foreach my $id (keys %affected) {
        if (!$votes{$id}) {
            push(@deleted_votes, $id);
            next;
        }
        if ($votes{$id} == ($old_votes{$id} || 0)) {
            delete $affected{$id};
            next;
        }
        # We use 'defined' in case 0 was accidentally stored in the DB.
        if (defined $old_votes{$id}) {
            $sth_updateVotes->execute($votes{$id}, $id, $who);
        }
        else {
            $sth_insertVotes->execute($who, $id, $votes{$id});
        }
    }

    if (@deleted_votes) {
        $dbh->do('DELETE FROM votes WHERE who = ? AND ' .
                 $dbh->sql_in('bug_id', \@deleted_votes), undef, $who);
    }

    # Update the cached values in the bugs table
    my @updated_bugs = ();

    my $sth_getVotes = $dbh->prepare("SELECT SUM(vote_count) FROM votes
                                      WHERE bug_id = ?");

    $sth_updateVotes = $dbh->prepare('UPDATE bugs SET votes = ? WHERE bug_id = ?');

    foreach my $id (keys %affected) {
        $sth_getVotes->execute($id);
        my $v = $sth_getVotes->fetchrow_array || 0;
        $sth_updateVotes->execute($v, $id);
        $bugs{$id}->{votes} = $v if $bugs{$id};
        my $confirmed = _confirm_if_vote_confirmed($bugs{$id} || $id);
        push (@updated_bugs, $id) if $confirmed;
    }

    $dbh->bz_commit_transaction();

    print $cgi->header() if scalar @updated_bugs;
    $vars->{'type'} = "votes";
    $vars->{'title_tag'} = 'change_votes';
    foreach my $bug_id (@updated_bugs) {
        $vars->{'id'} = $bug_id;
        $vars->{'sent_bugmail'} = 
            Bugzilla::BugMail::Send($bug_id, { 'changer' => $user });
        
        $template->process("bug/process/results.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        # Set header_done to 1 only after the first bug.
        $vars->{'header_done'} = 1;
    }
    $vars->{'message'} = 'votes_recorded';
}

######################
# Helper Subroutines #
######################

sub _modify_bug_votes {
    my ($product, $changes) = @_;
    my $dbh = Bugzilla->dbh;
    my @msgs;

    # 1. too many votes for a single user on a single bug.
    my @toomanyvotes_list;
    if ($product->{maxvotesperbug} < $product->{votesperuser}) {
        my $votes = $dbh->selectall_arrayref(
            'SELECT votes.who, votes.bug_id
               FROM votes
                    INNER JOIN bugs ON bugs.bug_id = votes.bug_id
              WHERE bugs.product_id = ?
                    AND votes.vote_count > ?',
            undef, ($product->id, $product->{maxvotesperbug}));

        foreach my $vote (@$votes) {
            my ($who, $id) = (@$vote);
            # If some votes are removed, _remove_votes() returns a list
            # of messages to send to voters.
            push(@msgs, _remove_votes($id, $who, 'votes_too_many_per_bug'));
            my $name = Bugzilla::User->new($who)->login;
            
            push(@toomanyvotes_list, {id => $id, name => $name});
        }
    }

    $changes->{'_too_many_votes'} = \@toomanyvotes_list;

    # 2. too many total votes for a single user.
    # This part doesn't work in the general case because _remove_votes
    # doesn't enforce votesperuser (except per-bug when it's less
    # than maxvotesperbug).  See _remove_votes().

    my $votes = $dbh->selectall_arrayref(
        'SELECT votes.who, votes.vote_count
           FROM votes
                INNER JOIN bugs ON bugs.bug_id = votes.bug_id
          WHERE bugs.product_id = ?',
         undef, $product->id);

    my %counts;
    foreach my $vote (@$votes) {
        my ($who, $count) = @$vote;
        if (!defined $counts{$who}) {
            $counts{$who} = $count;
        } else {
            $counts{$who} += $count;
        }
    }

    my @toomanytotalvotes_list;
    foreach my $who (keys(%counts)) {
        if ($counts{$who} > $product->{votesperuser}) {
            my $bug_ids = $dbh->selectcol_arrayref(
                'SELECT votes.bug_id
                   FROM votes
                        INNER JOIN bugs ON bugs.bug_id = votes.bug_id
                  WHERE bugs.product_id = ?
                        AND votes.who = ?',
                undef, $product->id, $who);

            my $name = Bugzilla::User->new($who)->login;
            foreach my $bug_id (@$bug_ids) {
                # _remove_votes returns a list of messages to send
                # in case some voters had too many votes.
                push(@msgs, _remove_votes($bug_id, $who, 
                                          'votes_too_many_per_user'));

                push(@toomanytotalvotes_list, {id => $bug_id, name => $name});
            }
        }
    }

    $changes->{'_too_many_total_votes'} = \@toomanytotalvotes_list;

    # 3. enough votes to confirm
    my $bug_list = $dbh->selectcol_arrayref(
        'SELECT bug_id FROM bugs 
          WHERE product_id = ? AND bug_status = ? AND votes >= ?',
        undef, ($product->id, 'UNCONFIRMED', $product->{votestoconfirm}));

    my @updated_bugs;
    foreach my $bug_id (@$bug_list) {
        my $confirmed = _confirm_if_vote_confirmed($bug_id);
        push (@updated_bugs, $bug_id) if $confirmed;
    }
    $changes->{'_confirmed_bugs'} = \@updated_bugs;

    # Now that changes are done, we can send emails to voters.
    foreach my $msg (@msgs) {
        MessageToMTA($msg);
    }
    # And send out emails about changed bugs
    foreach my $bug_id (@updated_bugs) {
        my $sent_bugmail = Bugzilla::BugMail::Send(
            $bug_id, { changer => Bugzilla->user });
        $changes->{'_confirmed_bugs_sent_bugmail'}->{$bug_id} = $sent_bugmail;
    }
}

# If a bug is moved to a product which allows less votes per bug
# compared to the previous product, extra votes need to be removed.
sub _remove_votes {
    my ($id, $who, $reason) = (@_);
    my $dbh = Bugzilla->dbh;

    my $whopart = ($who) ? " AND votes.who = $who" : "";

    my $sth = $dbh->prepare("SELECT profiles.login_name, " .
                            "profiles.userid, votes.vote_count, " .
                            "products.votesperuser, products.maxvotesperbug " .
                            "FROM profiles " .
                            "LEFT JOIN votes ON profiles.userid = votes.who " .
                            "LEFT JOIN bugs ON votes.bug_id = bugs.bug_id " .
                            "LEFT JOIN products ON products.id = bugs.product_id " .
                            "WHERE votes.bug_id = ? " . $whopart);
    $sth->execute($id);
    my @list;
    while (my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = $sth->fetchrow_array()) {
        push(@list, [$name, $userid, $oldvotes, $votesperuser, $maxvotesperbug]);
    }

    # @messages stores all emails which have to be sent, if any.
    # This array is passed to the caller which will send these emails itself.
    my @messages = ();

    if (scalar(@list)) {
        foreach my $ref (@list) {
            my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = (@$ref);

            $maxvotesperbug = min($votesperuser, $maxvotesperbug);

            # If this product allows voting and the user's votes are in
            # the acceptable range, then don't do anything.
            next if $votesperuser && $oldvotes <= $maxvotesperbug;

            # If the user has more votes on this bug than this product
            # allows, then reduce the number of votes so it fits
            my $newvotes = $maxvotesperbug;

            my $removedvotes = $oldvotes - $newvotes;

            if ($newvotes) {
                $dbh->do("UPDATE votes SET vote_count = ? " .
                         "WHERE bug_id = ? AND who = ?",
                         undef, ($newvotes, $id, $userid));
            } else {
                $dbh->do("DELETE FROM votes WHERE bug_id = ? AND who = ?",
                         undef, ($id, $userid));
            }

            # Notice that we did not make sure that the user fit within the $votesperuser
            # range.  This is considered to be an acceptable alternative to losing votes
            # during product moves.  Then next time the user attempts to change their votes,
            # they will be forced to fit within the $votesperuser limit.

            # Now lets send the e-mail to alert the user to the fact that their votes have
            # been reduced or removed.
            my $vars = {
                'to' => $name . Bugzilla->params->{'emailsuffix'},
                'bugid' => $id,
                'reason' => $reason,

                'votesremoved' => $removedvotes,
                'votesold' => $oldvotes,
                'votesnew' => $newvotes,
            };

            my $voter = new Bugzilla::User($userid);
            my $template = Bugzilla->template_inner($voter->setting('lang'));

            my $msg;
            $template->process("voting/votes-removed.txt.tmpl", $vars, \$msg);
            push(@messages, $msg);
        }

        my $votes = $dbh->selectrow_array("SELECT SUM(vote_count) " .
                                          "FROM votes WHERE bug_id = ?",
                                          undef, $id) || 0;
        $dbh->do("UPDATE bugs SET votes = ? WHERE bug_id = ?",
                 undef, ($votes, $id));
    }
    # Now return the array containing emails to be sent.
    return @messages;
}

# If a user votes for a bug, or the number of votes required to
# confirm a bug has been reduced, check if the bug is now confirmed.
sub _confirm_if_vote_confirmed {
    my $id = shift;
    my $bug = ref $id ? $id : new Bugzilla::Bug({ id => $id, cache => 1 });

    my $ret = 0;
    if (!$bug->everconfirmed
        and $bug->product_obj->{votestoconfirm}
        and $bug->votes >= $bug->product_obj->{votestoconfirm})
    {
        $bug->add_comment('', { type => CMT_POPULAR_VOTES });

        if ($bug->bug_status eq 'UNCONFIRMED') {
            # Get a valid open state.
            my $new_status;
            foreach my $state (@{$bug->status->can_change_to}) {
                if ($state->is_open && $state->name ne 'UNCONFIRMED') {
                    $new_status = $state->name;
                    last;
                }
            }
            ThrowCodeError('voting_no_open_bug_status') unless $new_status;

            # We cannot call $bug->set_bug_status() here, because a user without
            # canconfirm privs should still be able to confirm a bug by
            # popular vote. We already know the new status is valid, so it's safe.
            $bug->{bug_status} = $new_status;
            $bug->{everconfirmed} = 1;
            delete $bug->{'status'}; # Contains the status object.
        }
        else {
            # If the bug is in a closed state, only set everconfirmed to 1.
            # Do not call $bug->_set_everconfirmed(), for the same reason as above.
            $bug->{everconfirmed} = 1;
        }
        $bug->update();

        $ret = 1;
    }
    return $ret;
}


__PACKAGE__->NAME;
