# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Install;

# Functions in this this package can assume that the database 
# has been set up, params are available, localconfig is
# available, and any module can be used.
#
# If you want to write an installation function that can't
# make those assumptions, then it should go into one of the
# packages under the Bugzilla::Install namespace.

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Component;
use Bugzilla::Config qw(:admin);
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::User::Setting;
use Bugzilla::Util qw(get_text);
use Bugzilla::Version;

use constant STATUS_WORKFLOW => (
    [undef, 'UNCONFIRMED'],
    [undef, 'CONFIRMED'],
    [undef, 'IN_PROGRESS'],
    ['UNCONFIRMED', 'CONFIRMED'],
    ['UNCONFIRMED', 'IN_PROGRESS'],
    ['UNCONFIRMED', 'RESOLVED'],
    ['CONFIRMED',   'IN_PROGRESS'],
    ['CONFIRMED',   'RESOLVED'],
    ['IN_PROGRESS', 'CONFIRMED'],
    ['IN_PROGRESS', 'RESOLVED'],
    ['RESOLVED',    'UNCONFIRMED'],
    ['RESOLVED',    'CONFIRMED'],
    ['RESOLVED',    'VERIFIED'],
    ['VERIFIED',    'UNCONFIRMED'],
    ['VERIFIED',    'CONFIRMED'],
);

sub SETTINGS {
    return {
    # 2005-03-03 travis@sedsystems.ca -- Bug 41972
    display_quips      => { options => ["on", "off"], default => "on" },
    # 2005-03-10 travis@sedsystems.ca -- Bug 199048
    comment_sort_order => { options => ["oldest_to_newest", "newest_to_oldest",
                                        "newest_to_oldest_desc_first"],
                            default => "oldest_to_newest" },
    # 2005-05-12 bugzilla@glob.com.au -- Bug 63536
    post_bug_submit_action => { options => ["next_bug", "same_bug", "nothing"],
                                default => "next_bug" },
    # 2005-06-29 wurblzap@gmail.com -- Bug 257767
    csv_colsepchar     => { options => [',',';'], default => ',' },
    # 2005-10-26 wurblzap@gmail.com -- Bug 291459
    zoom_textareas     => { options => ["on", "off"], default => "on" },
    # 2006-05-01 olav@bkor.dhs.org -- Bug 7710
    state_addselfcc    => { options => ['always', 'never',  'cc_unless_role'],
                            default => 'cc_unless_role' },
    # 2006-08-04 wurblzap@gmail.com -- Bug 322693
    skin               => { subclass => 'Skin', default => 'Dusk' },
    # 2006-12-10 LpSolit@gmail.com -- Bug 297186
    lang               => { subclass => 'Lang',
                            default => ${Bugzilla->languages}[0] },
    # 2007-07-02 altlist@gmail.com -- Bug 225731
    quote_replies      => { options => ['quoted_reply', 'simple_reply', 'off'],
                            default => "quoted_reply" },
    # 2009-02-01 mozilla@matt.mchenryfamily.org -- Bug 398473
    comment_box_position => { options => ['before_comments', 'after_comments'],
                              default => 'before_comments' },
    # 2008-08-27 LpSolit@gmail.com -- Bug 182238
    timezone           => { subclass => 'Timezone', default => 'local' },
    # 2011-02-07 dkl@mozilla.com -- Bug 580490
    quicksearch_fulltext => { options => ['on', 'off'], default => 'on' },
    # 2011-06-21 glob@mozilla.com -- Bug 589128
    email_format       => { options => ['html', 'text_only'],
                            default => 'html' },
    # 2011-10-11 glob@mozilla.com -- Bug 301656
    requestee_cc       => { options => ['on', 'off'], default => 'on' },
    # 2012-04-30 glob@mozilla.com -- Bug 663747
    bugmail_new_prefix => { options => ['on', 'off'], default => 'on' },
    # 2013-07-26 joshi_sunil@in.com -- Bug 669535
    possible_duplicates => { options => ['on', 'off'], default => 'on' },
    }
};

use constant SYSTEM_GROUPS => (
    {
        name        => 'admin',
        description => 'Administrators'
    },
    {
        name        => 'tweakparams',
        description => 'Can change Parameters'
    },
    {
        name        => 'editusers',
        description => 'Can edit or disable users'
    },
    {
        name        => 'creategroups',
        description => 'Can create and destroy groups'
    },
    {
        name        => 'editclassifications',
        description => 'Can create, destroy, and edit classifications'
    },
    {
        name        => 'editcomponents',
        description => 'Can create, destroy, and edit components'
    },
    {
        name        => 'editkeywords',
        description => 'Can create, destroy, and edit keywords'
    },
    {
        name        => 'editbugs',
        description => 'Can edit all bug fields',
        userregexp  => '.*'
    },
    {
        name        => 'canconfirm',
        description => 'Can confirm a bug or mark it a duplicate'
    },
    {
        name         => 'bz_canusewhineatothers',
        description  => 'Can configure whine reports for other users',
    },
    {
        name         => 'bz_canusewhines',
        description  => 'User can configure whine reports for self',
        # inherited_by means that users in the groups listed below are
        # automatically members of bz_canusewhines.
        inherited_by => ['editbugs', 'bz_canusewhineatothers'],
    },
    {
        name        => 'bz_sudoers',
        description => 'Can perform actions as other users',
    },
    {
        name         => 'bz_sudo_protect',
        description  => 'Can not be impersonated by other users',
        inherited_by => ['bz_sudoers'],
    },
    {
        name         => 'bz_quip_moderators',
        description  => 'Can moderate quips',
    },
);

use constant DEFAULT_CLASSIFICATION => {
    name        => 'Unclassified',
    description => 'Not assigned to any classification'
};

use constant DEFAULT_PRODUCT => {
    name => 'TestProduct',
    description => 'This is a test product.'
        . ' This ought to be blown away and replaced with real stuff in a'
        . ' finished installation of bugzilla.',
    version => Bugzilla::Version::DEFAULT_VERSION,
    classification => 'Unclassified',
    defaultmilestone => DEFAULT_MILESTONE,
};

use constant DEFAULT_COMPONENT => {
    name => 'TestComponent',
    description => 'This is a test component in the test product database.'
        . ' This ought to be blown away and replaced with real stuff in'
        . ' a finished installation of Bugzilla.'
};

sub update_settings {
    my $dbh = Bugzilla->dbh;
    # If we're setting up settings for the first time, we want to be quieter.
    my $any_settings = $dbh->selectrow_array(
        'SELECT 1 FROM setting ' . $dbh->sql_limit(1));
    if (!$any_settings) {
        say get_text('install_setting_setup');
    }

    my %settings = %{SETTINGS()};
    foreach my $setting (keys %settings) {
        add_setting($setting,
                    $settings{$setting}->{options}, 
                    $settings{$setting}->{default},
                    $settings{$setting}->{subclass}, undef,
                    !$any_settings);
    }

    # Delete the obsolete 'per_bug_queries' user preference. Bug 616191.
    $dbh->do('DELETE FROM setting WHERE name = ?', undef, 'per_bug_queries');
}

sub update_system_groups {
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();

    # If there is no editbugs group, this is the first time we're
    # adding groups.
    my $editbugs_exists = new Bugzilla::Group({ name => 'editbugs' });
    if (!$editbugs_exists) {
        say get_text('install_groups_setup');
    }

    # Create most of the system groups
    foreach my $definition (SYSTEM_GROUPS) {
        my $exists = new Bugzilla::Group({ name => $definition->{name} });
        if (!$exists) {
            $definition->{isbuggroup} = 0;
            $definition->{silently} = !$editbugs_exists;
            my $inherited_by = delete $definition->{inherited_by};
            my $created = Bugzilla::Group->create($definition);
            # Each group in inherited_by is automatically a member of this
            # group.
            if ($inherited_by) {
                foreach my $name (@$inherited_by) {
                    my $member = Bugzilla::Group->check($name);
                    $dbh->do('INSERT INTO group_group_map (grantor_id, 
                                          member_id) VALUES (?,?)',
                             undef, $created->id, $member->id);
                }
            }
        }
    }

    $dbh->bz_commit_transaction();
}

sub create_default_classification {
    my $dbh = Bugzilla->dbh;

    # Make the default Classification if it doesn't already exist.
    if (!$dbh->selectrow_array('SELECT 1 FROM classifications')) {
        print get_text('install_default_classification',
                       { name => DEFAULT_CLASSIFICATION->{name} }) . "\n";
        Bugzilla::Classification->create(DEFAULT_CLASSIFICATION);
    }
}

# This function should be called only after creating the admin user.
sub create_default_product {
    my $dbh = Bugzilla->dbh;

    # And same for the default product/component.
    if (!$dbh->selectrow_array('SELECT 1 FROM products')) {
        print get_text('install_default_product', 
                       { name => DEFAULT_PRODUCT->{name} }) . "\n";

        my $product = Bugzilla::Product->create(DEFAULT_PRODUCT);

        # Get the user who will be the owner of the Component.
        # We pick the admin with the lowest id, which is probably the
        # admin checksetup.pl just created.
        my $admin_group = new Bugzilla::Group({name => 'admin'});
        my ($admin_id)  = $dbh->selectrow_array(
            'SELECT user_id FROM user_group_map WHERE group_id = ?
           ORDER BY user_id ' . $dbh->sql_limit(1),
            undef, $admin_group->id);
        my $admin = Bugzilla::User->new($admin_id);

        Bugzilla::Component->create({
            %{ DEFAULT_COMPONENT() }, product => $product,
            initialowner => $admin->login });
    }

}

sub init_workflow {
    my $dbh = Bugzilla->dbh;
    my $has_workflow = $dbh->selectrow_array('SELECT 1 FROM status_workflow');
    return if $has_workflow;

    say get_text('install_workflow_init');

    my %status_ids = @{ $dbh->selectcol_arrayref(
        'SELECT value, id FROM bug_status', {Columns=>[1,2]}) };

    foreach my $pair (STATUS_WORKFLOW) {
        my $old_id = $pair->[0] ? $status_ids{$pair->[0]} : undef;
        my $new_id = $status_ids{$pair->[1]};
        $dbh->do('INSERT INTO status_workflow (old_status, new_status)
                       VALUES (?,?)', undef, $old_id, $new_id);
    }
}

sub create_admin {
    my ($params) = @_;
    my $dbh      = Bugzilla->dbh;
    my $template = Bugzilla->template;

    my $admin_group = new Bugzilla::Group({ name => 'admin' });
    my $admin_inheritors = 
        Bugzilla::Group->flatten_group_membership($admin_group->id);
    my $admin_group_ids = join(',', @$admin_inheritors);

    my ($admin_count) = $dbh->selectrow_array(
        "SELECT COUNT(*) FROM user_group_map 
          WHERE group_id IN ($admin_group_ids)");

    return if $admin_count;

    my %answer    = %{Bugzilla->installation_answers};
    my $login     = $answer{'ADMIN_EMAIL'};
    my $password  = $answer{'ADMIN_PASSWORD'};
    my $full_name = $answer{'ADMIN_REALNAME'};

    if (!$login || !$password || !$full_name) {
        say "\n" . get_text('install_admin_setup') . "\n";
    }

    while (!$login) {
        print get_text('install_admin_get_email') . ' ';
        $login = <STDIN>;
        chomp $login;
        eval { Bugzilla::User->check_login_name($login); };
        if ($@) {
            say $@;
            undef $login;
        }
    }

    while (!defined $full_name) {
        print get_text('install_admin_get_name') . ' ';
        $full_name = <STDIN>;
        chomp($full_name);
    }

    if (!$password) {
        $password = _prompt_for_password(
            get_text('install_admin_get_password'));
    }

    my $admin = Bugzilla::User->create({ login_name    => $login, 
                                         realname      => $full_name,
                                         cryptpassword => $password });
    make_admin($admin);
}

sub make_admin {
    my ($user) = @_;
    my $dbh = Bugzilla->dbh;

    $user = ref($user) ? $user 
            : new Bugzilla::User(login_to_id($user, THROW_ERROR));

    my $group_insert = $dbh->prepare(
        'INSERT INTO user_group_map (user_id, group_id, isbless, grant_type)
              VALUES (?, ?, ?, ?)');

    # Admins get explicit membership and bless capability for the admin group
    my $admin_group = new Bugzilla::Group({ name => 'admin' });
    # These are run in an eval so that we can ignore the error of somebody
    # already being granted these things.
    eval { 
        $group_insert->execute($user->id, $admin_group->id, 0, GRANT_DIRECT); 
    };
    eval {
        $group_insert->execute($user->id, $admin_group->id, 1, GRANT_DIRECT);
    };

    # Admins should also have editusers directly, even though they'll usually
    # inherit it. People could have changed their inheritance structure.
    my $editusers = new Bugzilla::Group({ name => 'editusers' });
    eval { 
        $group_insert->execute($user->id, $editusers->id, 0, GRANT_DIRECT); 
    };

    # If there is no maintainer set, make this user the maintainer.
    if (!Bugzilla->params->{'maintainer'}) {
        SetParam('maintainer', $user->email);
        write_params();
    }

    # Make sure the new admin isn't disabled
    if ($user->disabledtext) {
        $user->set_disabledtext('');
        $user->update();
    }

    if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
        say "\n", get_text('install_admin_created', { user => $user });
    }
}

sub _prompt_for_password {
    my $prompt = shift;

    my $password;
    while (!$password) {
        # trap a few interrupts so we can fix the echo if we get aborted.
        local $SIG{HUP}  = \&_password_prompt_exit;
        local $SIG{INT}  = \&_password_prompt_exit;
        local $SIG{QUIT} = \&_password_prompt_exit;
        local $SIG{TERM} = \&_password_prompt_exit;

        system("stty","-echo") unless ON_WINDOWS;  # disable input echoing

        print $prompt, ' ';
        $password = <STDIN>;
        chomp $password;
        print "\n", get_text('install_confirm_password'), ' ';
        my $pass2 = <STDIN>;
        chomp $pass2;
        eval { validate_password($password, $pass2); };
        if ($@) {
            say "\n$@";
            undef $password;
        }
        system("stty","echo") unless ON_WINDOWS;
    }
    return $password;
}

# This is just in case we get interrupted while getting a password.
sub _password_prompt_exit {
    # re-enable input echoing
    system("stty","echo") unless ON_WINDOWS;
    exit 1;
}

sub reset_password {
    my $login = shift;
    my $user = Bugzilla::User->check($login);
    my $prompt = "\n" . get_text('install_reset_password', { user => $user });
    my $password = _prompt_for_password($prompt);
    $user->set_password($password);
    $user->update();
    say "\n", get_text('install_reset_password_done');
}

1;

__END__

=head1 NAME

Bugzilla::Install - Functions and variables having to do with
  installation.

=head1 SYNOPSIS

 use Bugzilla::Install;
 Bugzilla::Install::update_settings();

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> during installation.
This module contains functions that deal with general installation
issues after the database is completely set up and configured.

=head1 CONSTANTS

=over

=item C<SETTINGS>

Contains information about Settings, used by L</update_settings()>.

=back

=head1 SUBROUTINES

=over

=item C<update_settings()>

Description: Adds and updates Settings for users.

Params:      none

Returns:     nothing.

=item C<create_default_classification>

Creates the default "Unclassified" L<Classification|Bugzilla::Classification>
if it doesn't already exist

=item C<create_default_product()>

Description: Creates the default product and component if
             they don't exist.

Params:      none

Returns:     nothing

=back

=head1 B<Methods in need of POD>

=over

=item update_system_groups

=item reset_password

=item make_admin

=item create_admin

=item init_workflow

=back
