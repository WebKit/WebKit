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
use Bugzilla::Bug;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Hook;
use Bugzilla::Util;
use Bugzilla::Status;
use Bugzilla::Token;

###########################################################################
# General subs
###########################################################################

sub get_string {
    my ($san_tag, $vars) = @_;
    $vars->{'san_tag'} = $san_tag;
    return get_text('sanitycheck', $vars);
}

sub Status {
    my ($san_tag, $vars, $alert) = @_;
    my $cgi = Bugzilla->cgi;
    return if (!$alert && Bugzilla->usage_mode == USAGE_MODE_CMDLINE && !$cgi->param('verbose'));

    if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
        my $output = $cgi->param('output') || '';
        my $linebreak = $alert ? "\nALERT: " : "\n";
        $cgi->param('error_found', 1) if $alert;
        $cgi->param('output', $output . $linebreak . get_string($san_tag, $vars));
    }
    else {
        my $start_tag = $alert ? '<p class="alert">' : '<p>';
        say $start_tag . get_string($san_tag, $vars) . "</p>";
    }
}

###########################################################################
# Start
###########################################################################

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
# If the result of the sanity check is sent per email, then we have to
# take the user prefs into account rather than querying the web browser.
my $template;
if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
    $template = Bugzilla->template_inner($user->setting('lang'));
}
else {
    $template = Bugzilla->template;

    # Only check the token if we are running this script from the
    # web browser and a parameter is passed to the script.
    # XXX - Maybe these two parameters should be deleted once logged in?
    $cgi->delete('GoAheadAndLogIn', 'Bugzilla_restrictlogin');
    if (scalar($cgi->param())) {
        my $token = $cgi->param('token');
        check_hash_token($token, ['sanitycheck']);
    }
}
my $vars = {};
my $clear_memcached = 0;

print $cgi->header() unless Bugzilla->usage_mode == USAGE_MODE_CMDLINE;

# Make sure the user is authorized to access sanitycheck.cgi.
# As this script can now alter the group_control_map table, we no longer
# let users with editbugs privs run it anymore.
$user->in_group("editcomponents")
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "run",
                                     object => "sanity_check"});

unless (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
    $template->process('admin/sanitycheck/list.html.tmpl', $vars)
      || ThrowTemplateError($template->error());
}

###########################################################################
# Create missing group_control_map entries
###########################################################################

if ($cgi->param('createmissinggroupcontrolmapentries')) {
    Status('group_control_map_entries_creation');

    my $na    = CONTROLMAPNA;
    my $shown = CONTROLMAPSHOWN;
    my $insertsth = $dbh->prepare(
        qq{INSERT INTO group_control_map
                       (group_id, product_id, membercontrol, othercontrol)
                VALUES (?, ?, $shown, $na)});

    my $updatesth = $dbh->prepare(qq{UPDATE group_control_map
                                        SET membercontrol = $shown
                                      WHERE group_id   = ?
                                        AND product_id = ?});
    my $counter = 0;

    # Find all group/product combinations used for bugs but not set up
    # correctly in group_control_map
    my $invalid_combinations = $dbh->selectall_arrayref(
        qq{    SELECT bugs.product_id,
                      bgm.group_id,
                      gcm.membercontrol,
                      groups.name,
                      products.name
                 FROM bugs
           INNER JOIN bug_group_map AS bgm
                   ON bugs.bug_id = bgm.bug_id
           INNER JOIN groups
                   ON bgm.group_id = groups.id
           INNER JOIN products
                   ON bugs.product_id = products.id
            LEFT JOIN group_control_map AS gcm
                   ON bugs.product_id = gcm.product_id
                  AND    bgm.group_id = gcm.group_id
                WHERE COALESCE(gcm.membercontrol, $na) = $na
          } . $dbh->sql_group_by('bugs.product_id, bgm.group_id',
                                 'gcm.membercontrol, groups.name, products.name'));

    foreach (@$invalid_combinations) {
        my ($product_id, $group_id, $currentmembercontrol,
            $group_name, $product_name) = @$_;

        $counter++;
        if (defined($currentmembercontrol)) {
            Status('group_control_map_entries_update',
                   {group_name => $group_name, product_name => $product_name});
            $updatesth->execute($group_id, $product_id);
        }
        else {
            Status('group_control_map_entries_generation',
                   {group_name => $group_name, product_name => $product_name});
            $insertsth->execute($group_id, $product_id);
        }
    }

    Status('group_control_map_entries_repaired', {counter => $counter});
    $clear_memcached = 1 if $counter;
}

###########################################################################
# Fix missing creation date
###########################################################################

if ($cgi->param('repair_creation_date')) {
    Status('bug_creation_date_start');

    my $bug_ids = $dbh->selectcol_arrayref('SELECT bug_id FROM bugs
                                            WHERE creation_ts IS NULL');

    my $sth_UpdateDate = $dbh->prepare('UPDATE bugs SET creation_ts = ?
                                        WHERE bug_id = ?');

    # All bugs have an entry in the 'longdescs' table when they are created,
    # even if no comment is required.
    my $sth_getDate = $dbh->prepare('SELECT MIN(bug_when) FROM longdescs
                                     WHERE bug_id = ?');

    foreach my $bugid (@$bug_ids) {
        $sth_getDate->execute($bugid);
        my $date = $sth_getDate->fetchrow_array;
        $sth_UpdateDate->execute($date, $bugid);
    }
    Status('bug_creation_date_fixed', {bug_count => scalar(@$bug_ids)});
    $clear_memcached = 1 if @$bug_ids;
}

###########################################################################
# Fix everconfirmed
###########################################################################

if ($cgi->param('repair_everconfirmed')) {
    Status('everconfirmed_start');

    my @confirmed_open_states = grep {$_ ne 'UNCONFIRMED'} BUG_STATE_OPEN;
    my $confirmed_open_states = join(', ', map {$dbh->quote($_)} @confirmed_open_states);

    $dbh->do("UPDATE bugs SET everconfirmed = 0 WHERE bug_status = 'UNCONFIRMED'");
    $dbh->do("UPDATE bugs SET everconfirmed = 1 WHERE bug_status IN ($confirmed_open_states)");

    Status('everconfirmed_end');
    $clear_memcached = 1;
}

###########################################################################
# Fix entries in Bugs full_text
###########################################################################

if ($cgi->param('repair_bugs_fulltext')) {
    Status('bugs_fulltext_start');

    my $bug_ids = $dbh->selectcol_arrayref('SELECT bugs.bug_id
                                            FROM bugs
                                            LEFT JOIN bugs_fulltext
                                            ON bugs_fulltext.bug_id = bugs.bug_id
                                            WHERE bugs_fulltext.bug_id IS NULL');

    foreach my $bugid (@$bug_ids) {
        Bugzilla::Bug->new($bugid)->_sync_fulltext( new_bug => 1 );
    }

    Status('bugs_fulltext_fixed', {bug_count => scalar(@$bug_ids)});
    $clear_memcached = 1 if @$bug_ids;
}

###########################################################################
# Send unsent mail
###########################################################################

if ($cgi->param('rescanallBugMail')) {
    require Bugzilla::BugMail;

    Status('send_bugmail_start');
    my $time = $dbh->sql_date_math('NOW()', '-', 30, 'MINUTE');

    my $list = $dbh->selectcol_arrayref(qq{
                                        SELECT bug_id
                                          FROM bugs 
                                         WHERE (lastdiffed IS NULL
                                                OR lastdiffed < delta_ts)
                                           AND delta_ts < $time
                                      ORDER BY bug_id});

    Status('send_bugmail_status', {bug_count => scalar(@$list)});

    # We cannot simply look at the bugs_activity table to find who did the
    # last change in a given bug, as e.g. adding a comment doesn't add any
    # entry to this table. And some other changes may be private
    # (such as time-related changes or private attachments or comments)
    # and so choosing this user as being the last one having done a change
    # for the bug may be problematic. So the best we can do at this point
    # is to choose the currently logged in user for email notification.
    $vars->{'changer'} = $user;

    foreach my $bugid (@$list) {
        Bugzilla::BugMail::Send($bugid, $vars);
    }

    if (@$list) {
        Status('send_bugmail_end');
        Bugzilla->memcached->clear_all();
    }

    unless (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
        $template->process('global/footer.html.tmpl', $vars)
          || ThrowTemplateError($template->error());
    }
    exit;
}

###########################################################################
# Remove all references to deleted bugs
###########################################################################

if ($cgi->param('remove_invalid_bug_references')) {
    Status('bug_reference_deletion_start');

    $dbh->bz_start_transaction();

    foreach my $pair ('attachments/', 'bug_group_map/', 'bugs_activity/',
                      'bugs_fulltext/', 'cc/',
                      'dependencies/blocked', 'dependencies/dependson',
                      'duplicates/dupe', 'duplicates/dupe_of',
                      'flags/', 'keywords/', 'longdescs/') {

        my ($table, $field) = split('/', $pair);
        $field ||= "bug_id";

        my $bug_ids =
          $dbh->selectcol_arrayref("SELECT $table.$field FROM $table
                                    LEFT JOIN bugs ON $table.$field = bugs.bug_id
                                    WHERE bugs.bug_id IS NULL");

        if (scalar(@$bug_ids)) {
            $dbh->do("DELETE FROM $table WHERE $field IN (" . join(',', @$bug_ids) . ")");
            $clear_memcached = 1;
        }
    }

    $dbh->bz_commit_transaction();
    Status('bug_reference_deletion_end');
}

###########################################################################
# Remove all references to deleted attachments
###########################################################################

if ($cgi->param('remove_invalid_attach_references')) {
    Status('attachment_reference_deletion_start');

    $dbh->bz_start_transaction();

    my $attach_ids =
        $dbh->selectcol_arrayref('SELECT attach_data.id
                                    FROM attach_data
                               LEFT JOIN attachments
                                      ON attachments.attach_id = attach_data.id
                                   WHERE attachments.attach_id IS NULL');

    if (scalar(@$attach_ids)) {
        $dbh->do('DELETE FROM attach_data WHERE id IN (' .
                 join(',', @$attach_ids) . ')');
    }

    $dbh->bz_commit_transaction();
    Status('attachment_reference_deletion_end');
    $clear_memcached = 1 if @$attach_ids;
}

###########################################################################
# Remove all references to deleted users or groups from whines
###########################################################################

if ($cgi->param('remove_old_whine_targets')) {
    Status('whines_obsolete_target_deletion_start');

    $dbh->bz_start_transaction();

    foreach my $target (['groups', 'id', MAILTO_GROUP],
                        ['profiles', 'userid', MAILTO_USER])
    {
        my ($table, $col, $type) = @$target;
        my $old_ids =
          $dbh->selectcol_arrayref("SELECT DISTINCT mailto
                                      FROM whine_schedules
                                 LEFT JOIN $table
                                        ON $table.$col = whine_schedules.mailto
                                     WHERE mailto_type = $type AND $table.$col IS NULL");

        if (scalar(@$old_ids)) {
            $dbh->do("DELETE FROM whine_schedules
                       WHERE mailto_type = $type AND mailto IN (" .
                       join(',', @$old_ids) . ")");
            $clear_memcached = 1;
        }
    }
    $dbh->bz_commit_transaction();
    Status('whines_obsolete_target_deletion_end');
}

# If any repairs were attempted or made, we need to clear memcached to ensure
# state is consistent.
Bugzilla->memcached->clear_all() if $clear_memcached;

###########################################################################
# Repair hook
###########################################################################

Bugzilla::Hook::process('sanitycheck_repair', { status => \&Status });

###########################################################################
# Checks
###########################################################################
Status('checks_start');

###########################################################################
# Perform referential (cross) checks
###########################################################################

# This checks that a simple foreign key has a valid primary key value.  NULL
# references are acceptable and cause no problem.
#
# The first parameter is the primary key table name.
# The second parameter is the primary key field name.
# Each successive parameter represents a foreign key, it must be a list
# reference, where the list has:
#   the first value is the foreign key table name.
#   the second value is the foreign key field name.
#   the third value is optional and represents a field on the foreign key
#     table to display when the check fails.
#   the fourth value is optional and is a list reference to values that
#     are excluded from checking.
#
# FIXME: The excluded values parameter should go away - the QA contact
#        fields should use NULL instead - see bug #109474.
#        The same goes for series; no bug for that yet.

sub CrossCheck {
    my $table = shift @_;
    my $field = shift @_;
    my $dbh = Bugzilla->dbh;

    Status('cross_check_to', {table => $table, field => $field});

    while (@_) {
        my $ref = shift @_;
        my ($refertable, $referfield, $keyname, $exceptions) = @$ref;

        $exceptions ||= [];
        my %exceptions = map { $_ => 1 } @$exceptions;

        Status('cross_check_from', {table => $refertable, field => $referfield});

        my $query = qq{SELECT DISTINCT $refertable.$referfield} .
            ($keyname ? qq{, $refertable.$keyname } : q{}) .
                     qq{ FROM $refertable
                    LEFT JOIN $table
                           ON $refertable.$referfield = $table.$field
                        WHERE $table.$field IS NULL
                          AND $refertable.$referfield IS NOT NULL};

        my $sth = $dbh->prepare($query);
        $sth->execute;

        my $has_bad_references = 0;

        while (my ($value, $key) = $sth->fetchrow_array) {
            next if $exceptions{$value};
            Status('cross_check_alert', {value => $value, table => $refertable,
                                         field => $referfield, keyname => $keyname,
                                         key => $key}, 'alert');
            $has_bad_references = 1;
        }
        # References to non existent bugs can be safely removed, bug 288461
        if ($table eq 'bugs' && $has_bad_references) {
            Status('cross_check_bug_has_references');
        }
        # References to non existent attachments can be safely removed.
        if ($table eq 'attachments' && $has_bad_references) {
            Status('cross_check_attachment_has_references');
        }
    }
}

CrossCheck('classifications', 'id',
           ['products', 'classification_id']);

CrossCheck("keyworddefs", "id",
           ["keywords", "keywordid"]);

CrossCheck("fielddefs", "id",
           ["bugs_activity", "fieldid"],
           ['profiles_activity', 'fieldid']);

CrossCheck("flagtypes", "id",
           ["flags", "type_id"],
           ["flagexclusions", "type_id"],
           ["flaginclusions", "type_id"]);

CrossCheck("bugs", "bug_id",
           ["bugs_activity", "bug_id"],
           ["bug_group_map", "bug_id"],
           ["bugs_fulltext", "bug_id"],
           ["attachments", "bug_id"],
           ["cc", "bug_id"],
           ["longdescs", "bug_id"],
           ["dependencies", "blocked"],
           ["dependencies", "dependson"],
           ['flags', 'bug_id'],
           ["keywords", "bug_id"],
           ["duplicates", "dupe_of", "dupe"],
           ["duplicates", "dupe", "dupe_of"]);

CrossCheck("groups", "id",
           ["bug_group_map", "group_id"],
           ['category_group_map', 'group_id'],
           ["group_group_map", "grantor_id"],
           ["group_group_map", "member_id"],
           ["group_control_map", "group_id"],
           ["namedquery_group_map", "group_id"],
           ["user_group_map", "group_id"],
           ["flagtypes", "grant_group_id"],
           ["flagtypes", "request_group_id"]);

CrossCheck("namedqueries", "id",
           ["namedqueries_link_in_footer", "namedquery_id"],
           ["namedquery_group_map", "namedquery_id"],
          );

CrossCheck("profiles", "userid",
           ['profiles_activity', 'userid'],
           ['profiles_activity', 'who'],
           ['email_setting', 'user_id'],
           ['profile_setting', 'user_id'],
           ["bugs", "reporter", "bug_id"],
           ["bugs", "assigned_to", "bug_id"],
           ["bugs", "qa_contact", "bug_id"],
           ["attachments", "submitter_id", "bug_id"],
           ['flags', 'setter_id', 'bug_id'],
           ['flags', 'requestee_id', 'bug_id'],
           ["bugs_activity", "who", "bug_id"],
           ["cc", "who", "bug_id"],
           ['quips', 'userid'],
           ["longdescs", "who", "bug_id"],
           ["logincookies", "userid"],
           ["namedqueries", "userid"],
           ["namedqueries_link_in_footer", "user_id"],
           ['series', 'creator', 'series_id'],
           ["watch", "watcher"],
           ["watch", "watched"],
           ['whine_events', 'owner_userid'],
           ["tokens", "userid"],
           ["user_group_map", "user_id"],
           ["components", "initialowner", "name"],
           ["components", "initialqacontact", "name"],
           ["component_cc", "user_id"]);

CrossCheck("products", "id",
           ["bugs", "product_id", "bug_id"],
           ["components", "product_id", "name"],
           ["milestones", "product_id", "value"],
           ["versions", "product_id", "value"],
           ["group_control_map", "product_id"],
           ["flaginclusions", "product_id", "type_id"],
           ["flagexclusions", "product_id", "type_id"]);

CrossCheck("components", "id",
           ["component_cc", "component_id"],
           ["flagexclusions", "component_id", "type_id"],
           ["flaginclusions", "component_id", "type_id"]);

# Check the former enum types -mkanat@bugzilla.org
CrossCheck("bug_status", "value",
            ["bugs", "bug_status", "bug_id"]);

CrossCheck("resolution", "value",
            ["bugs", "resolution", "bug_id"]);

CrossCheck("bug_severity", "value",
            ["bugs", "bug_severity", "bug_id"]);

CrossCheck("op_sys", "value",
            ["bugs", "op_sys", "bug_id"]);

CrossCheck("priority", "value",
            ["bugs", "priority", "bug_id"]);

CrossCheck("rep_platform", "value",
            ["bugs", "rep_platform", "bug_id"]);

CrossCheck('series', 'series_id',
           ['series_data', 'series_id']);

CrossCheck('series_categories', 'id',
           ['series', 'category'],
           ["category_group_map", "category_id"],
           ["series", "subcategory"]);

CrossCheck('whine_events', 'id',
           ['whine_queries', 'eventid'],
           ['whine_schedules', 'eventid']);

CrossCheck('attachments', 'attach_id',
           ['attach_data', 'id'],
           ['bugs_activity', 'attach_id']);

CrossCheck('bug_status', 'id',
           ['status_workflow', 'old_status'],
           ['status_workflow', 'new_status']);

###########################################################################
# Perform double field referential (cross) checks
###########################################################################
 
# This checks that a compound two-field foreign key has a valid primary key
# value.  NULL references are acceptable and cause no problem.
#
# The first parameter is the primary key table name.
# The second parameter is the primary key first field name.
# The third parameter is the primary key second field name.
# Each successive parameter represents a foreign key, it must be a list
# reference, where the list has:
#   the first value is the foreign key table name
#   the second value is the foreign key first field name.
#   the third value is the foreign key second field name.
#   the fourth value is optional and represents a field on the foreign key
#     table to display when the check fails

sub DoubleCrossCheck {
    my $table = shift @_;
    my $field1 = shift @_;
    my $field2 = shift @_;
    my $dbh = Bugzilla->dbh;

    Status('double_cross_check_to',
           {table => $table, field1 => $field1, field2 => $field2});

    while (@_) {
        my $ref = shift @_;
        my ($refertable, $referfield1, $referfield2, $keyname) = @$ref;

        Status('double_cross_check_from',
               {table => $refertable, field1 => $referfield1, field2 =>$referfield2});

        my $d_cross_check = $dbh->selectall_arrayref(qq{
                        SELECT DISTINCT $refertable.$referfield1, 
                                        $refertable.$referfield2 } .
                       ($keyname ? qq{, $refertable.$keyname } : q{}) .
                      qq{ FROM $refertable
                     LEFT JOIN $table
                            ON $refertable.$referfield1 = $table.$field1
                           AND $refertable.$referfield2 = $table.$field2 
                         WHERE $table.$field1 IS NULL 
                           AND $table.$field2 IS NULL 
                           AND $refertable.$referfield1 IS NOT NULL 
                           AND $refertable.$referfield2 IS NOT NULL});

        foreach my $check (@$d_cross_check) {
            my ($value1, $value2, $key) = @$check;
            Status('double_cross_check_alert',
                   {value1 => $value1, value2 => $value2,
                    table => $refertable,
                    field1 => $referfield1, field2 => $referfield2,
                    keyname => $keyname, key => $key}, 'alert');
        }
    }
}

DoubleCrossCheck('attachments', 'bug_id', 'attach_id',
                 ['flags', 'bug_id', 'attach_id'],
                 ['bugs_activity', 'bug_id', 'attach_id']);

DoubleCrossCheck("components", "product_id", "id",
                 ["bugs", "product_id", "component_id", "bug_id"],
                 ['flagexclusions', 'product_id', 'component_id'],
                 ['flaginclusions', 'product_id', 'component_id']);

DoubleCrossCheck("versions", "product_id", "value",
                 ["bugs", "product_id", "version", "bug_id"]);
 
DoubleCrossCheck("milestones", "product_id", "value",
                 ["bugs", "product_id", "target_milestone", "bug_id"],
                 ["products", "id", "defaultmilestone", "name"]);

###########################################################################
# Perform login checks
###########################################################################

Status('profile_login_start');

my $sth = $dbh->prepare(q{SELECT userid, login_name FROM profiles});
$sth->execute;

while (my ($id, $email) = $sth->fetchrow_array) {
    validate_email_syntax($email)
      || Status('profile_login_alert', {id => $id, email => $email}, 'alert');
}

###########################################################################
# Perform keyword checks
###########################################################################

sub check_keywords {
    my $dbh = Bugzilla->dbh;
    my $cgi = Bugzilla->cgi;

    Status('keyword_check_start');

    my %keywordids;
    my $keywords = $dbh->selectall_arrayref(q{SELECT id, name
                                                FROM keyworddefs});

    foreach (@$keywords) {
        my ($id, $name) = @$_;
        if ($keywordids{$id}) {
            Status('keyword_check_alert', {id => $id}, 'alert');
        }
        $keywordids{$id} = 1;
        if ($name =~ /[\s,]/) {
            Status('keyword_check_invalid_name', {id => $id}, 'alert');
        }
    }

    my $sth = $dbh->prepare(q{SELECT bug_id, keywordid
                                FROM keywords
                            ORDER BY bug_id, keywordid});
    $sth->execute;
    my $lastid;
    my $lastk;
    while (my ($id, $k) = $sth->fetchrow_array) {
        if (!$keywordids{$k}) {
            Status('keyword_check_invalid_id', {id => $k}, 'alert');
        }
        if (defined $lastid && $id eq $lastid && $k eq $lastk) {
            Status('keyword_check_duplicated_ids', {id => $id}, 'alert');
        }
        $lastid = $id;
        $lastk = $k;
    }
}

###########################################################################
# Check for flags being in incorrect products and components
###########################################################################

Status('flag_check_start');

my $invalid_flags = $dbh->selectall_arrayref(
       'SELECT DISTINCT flags.id, flags.bug_id, flags.attach_id
          FROM flags
    INNER JOIN bugs
            ON flags.bug_id = bugs.bug_id
     LEFT JOIN flaginclusions AS i
            ON flags.type_id = i.type_id
           AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
           AND (bugs.component_id = i.component_id OR i.component_id IS NULL)
         WHERE i.type_id IS NULL');

my @invalid_flags = @$invalid_flags;

$invalid_flags = $dbh->selectall_arrayref(
       'SELECT DISTINCT flags.id, flags.bug_id, flags.attach_id
          FROM flags
    INNER JOIN bugs
            ON flags.bug_id = bugs.bug_id
    INNER JOIN flagexclusions AS e
            ON flags.type_id = e.type_id
         WHERE (bugs.product_id = e.product_id OR e.product_id IS NULL)
           AND (bugs.component_id = e.component_id OR e.component_id IS NULL)');

push(@invalid_flags, @$invalid_flags);

if (scalar(@invalid_flags)) {
    if ($cgi->param('remove_invalid_flags')) {
        Status('flag_deletion_start');
        my @flag_ids = map {$_->[0]} @invalid_flags;
        # Silently delete these flags, with no notification to requesters/setters.
        $dbh->do('DELETE FROM flags WHERE id IN (' . join(',', @flag_ids) .')');
        Status('flag_deletion_end');
        Bugzilla->memcached->clear_all();
    }
    else {
        foreach my $flag (@$invalid_flags) {
            my ($flag_id, $bug_id, $attach_id) = @$flag;
            Status('flag_alert',
                   {flag_id => $flag_id, attach_id => $attach_id, bug_id => $bug_id},
                   'alert');
        }
        Status('flag_fix');
    }
}

###########################################################################
# Check for products with no component
###########################################################################

Status('product_check_start');

my $products_missing_data = $dbh->selectcol_arrayref(
      'SELECT DISTINCT products.name
         FROM products
    LEFT JOIN components
           ON components.product_id = products.id
    LEFT JOIN versions
           ON versions.product_id = products.id
        WHERE components.id IS NULL
           OR versions.id IS NULL');

if (scalar(@$products_missing_data)) {
    Status('product_alert', { name => $_ }, 'alert') foreach @$products_missing_data;
}

###########################################################################
# General bug checks
###########################################################################

sub BugCheck {
    my ($middlesql, $errortext, $repairparam, $repairtext) = @_;
    my $dbh = Bugzilla->dbh;
 
    my $badbugs = $dbh->selectcol_arrayref(qq{SELECT DISTINCT bugs.bug_id
                                                FROM $middlesql 
                                            ORDER BY bugs.bug_id});

    if (scalar(@$badbugs)) {
        Status('bug_check_alert',
               {errortext => get_string($errortext), badbugs => $badbugs},
               'alert');

        if ($repairparam) {
            $repairtext ||= 'repair_bugs';
            Status('bug_check_repair',
                   {param => $repairparam, text => get_string($repairtext)});
        }
    }
}

Status('bug_check_creation_date');

BugCheck("bugs WHERE creation_ts IS NULL", 'bug_check_creation_date_error_text',
         'repair_creation_date', 'bug_check_creation_date_repair_text');

Status('bug_check_bugs_fulltext');

BugCheck("bugs LEFT JOIN bugs_fulltext ON bugs_fulltext.bug_id = bugs.bug_id " .
         "WHERE bugs_fulltext.bug_id IS NULL", 'bug_check_bugs_fulltext_error_text',
         'repair_bugs_fulltext', 'bug_check_bugs_fulltext_repair_text');

Status('bug_check_res_dupl');

BugCheck("bugs INNER JOIN duplicates ON bugs.bug_id = duplicates.dupe " .
         "WHERE bugs.resolution != 'DUPLICATE'", 'bug_check_res_dupl_error_text');

BugCheck("bugs LEFT JOIN duplicates ON bugs.bug_id = duplicates.dupe WHERE " .
         "bugs.resolution = 'DUPLICATE' AND " .
         "duplicates.dupe IS NULL", 'bug_check_res_dupl_error_text2');

Status('bug_check_status_res');

my @open_states = map($dbh->quote($_), BUG_STATE_OPEN);
my $open_states = join(', ', @open_states);

BugCheck("bugs WHERE bug_status IN ($open_states) AND resolution != ''",
         'bug_check_status_res_error_text');
BugCheck("bugs WHERE bug_status NOT IN ($open_states) AND resolution = ''",
         'bug_check_status_res_error_text2');

Status('bug_check_status_everconfirmed');

BugCheck("bugs WHERE bug_status = 'UNCONFIRMED' AND everconfirmed = 1",
         'bug_check_status_everconfirmed_error_text', 'repair_everconfirmed');

my @confirmed_open_states = grep {$_ ne 'UNCONFIRMED'} BUG_STATE_OPEN;
my $confirmed_open_states = join(', ', map {$dbh->quote($_)} @confirmed_open_states);

BugCheck("bugs WHERE bug_status IN ($confirmed_open_states) AND everconfirmed = 0",
         'bug_check_status_everconfirmed_error_text2', 'repair_everconfirmed');

###########################################################################
# Control Values
###########################################################################

# Checks for values that are invalid OR
# not among the 9 valid combinations
Status('bug_check_control_values');
my $groups = join(", ", (CONTROLMAPNA, CONTROLMAPSHOWN, CONTROLMAPDEFAULT,
CONTROLMAPMANDATORY));
my $query = qq{
     SELECT COUNT(product_id) 
       FROM group_control_map 
      WHERE membercontrol NOT IN( $groups )
         OR othercontrol NOT IN( $groups )
         OR ((membercontrol != othercontrol)
             AND (membercontrol != } . CONTROLMAPSHOWN . q{)
             AND ((membercontrol != } . CONTROLMAPDEFAULT . q{)
                  OR (othercontrol = } . CONTROLMAPSHOWN . q{)))};

my $entries = $dbh->selectrow_array($query);
Status('bug_check_control_values_alert', {entries => $entries}, 'alert') if $entries;

Status('bug_check_control_values_violation');
BugCheck("bugs
         INNER JOIN bug_group_map
            ON bugs.bug_id = bug_group_map.bug_id
          LEFT JOIN group_control_map
            ON bugs.product_id = group_control_map.product_id
           AND bug_group_map.group_id = group_control_map.group_id
         WHERE ((group_control_map.membercontrol = " . CONTROLMAPNA . ")
         OR (group_control_map.membercontrol IS NULL))",
         'bug_check_control_values_error_text',
         'createmissinggroupcontrolmapentries',
         'bug_check_control_values_repair_text');

BugCheck("bugs
         INNER JOIN group_control_map
            ON bugs.product_id = group_control_map.product_id
         INNER JOIN groups
            ON group_control_map.group_id = groups.id
          LEFT JOIN bug_group_map
            ON bugs.bug_id = bug_group_map.bug_id
           AND group_control_map.group_id = bug_group_map.group_id
         WHERE group_control_map.membercontrol = " . CONTROLMAPMANDATORY . "
           AND bug_group_map.group_id IS NULL
           AND groups.isactive != 0",
         'bug_check_control_values_error_text2');

###########################################################################
# Unsent mail
###########################################################################

Status('unsent_bugmail_check');

my $time = $dbh->sql_date_math('NOW()', '-', 30, 'MINUTE');
my $badbugs = $dbh->selectcol_arrayref(qq{
                    SELECT bug_id 
                      FROM bugs 
                     WHERE (lastdiffed IS NULL OR lastdiffed < delta_ts)
                       AND delta_ts < $time
                  ORDER BY bug_id});


if (scalar(@$badbugs > 0)) {
    Status('unsent_bugmail_alert', {badbugs => $badbugs}, 'alert');
    Status('unsent_bugmail_fix');
}

###########################################################################
# Whines
###########################################################################

Status('whines_obsolete_target_start');

my $display_repair_whines_link = 0;
foreach my $target (['groups', 'id', MAILTO_GROUP],
                    ['profiles', 'userid', MAILTO_USER])
{
    my ($table, $col, $type) = @$target;
    my $old = $dbh->selectall_arrayref("SELECT whine_schedules.id, mailto
                                          FROM whine_schedules
                                     LEFT JOIN $table
                                            ON $table.$col = whine_schedules.mailto
                                         WHERE mailto_type = $type AND $table.$col IS NULL");

    if (scalar(@$old)) {
        Status('whines_obsolete_target_alert', {schedules => $old, type => $type}, 'alert');
        $display_repair_whines_link = 1;
    }
}
Status('whines_obsolete_target_fix') if $display_repair_whines_link;

###########################################################################
# Check hook
###########################################################################

Bugzilla::Hook::process('sanitycheck_check', { status => \&Status });

###########################################################################
# End
###########################################################################

Status('checks_completed');

unless (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
    $template->process('global/footer.html.tmpl', $vars)
      || ThrowTemplateError($template->error());
}
