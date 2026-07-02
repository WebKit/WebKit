# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Install::DB;

# NOTE: This package may "use" any modules that it likes,
# localconfig is available, and params are up to date. 

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Hook;
use Bugzilla::Install ();
use Bugzilla::Install::Util qw(indicate_progress install_string);
use Bugzilla::Util;
use Bugzilla::Series;
use Bugzilla::BugUrl;
use Bugzilla::Field;

use Date::Parse;
use Date::Format;
use Digest;
use IO::File;
use List::MoreUtils qw(uniq);
use URI;
use URI::QueryParam;

# NOTE: This is NOT the function for general table updates. See
# update_table_definitions for that. This is only for the fielddefs table.
sub update_fielddefs_definition {
    my $dbh = Bugzilla->dbh;

    # 2005-02-21 - LpSolit@gmail.com - Bug 279910
    # qacontact_accessible and assignee_accessible field names no longer exist
    # in the 'bugs' table. Their corresponding entries in the 'bugs_activity'
    # table should therefore be marked as obsolete, meaning that they cannot
    # be used anymore when querying the database - they are not deleted in
    # order to keep track of these fields in the activity table.
    if (!$dbh->bz_column_info('fielddefs', 'obsolete')) {
        $dbh->bz_add_column('fielddefs', 'obsolete',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
        print "Marking qacontact_accessible and assignee_accessible as",
              " obsolete fields...\n";
        $dbh->do("UPDATE fielddefs SET obsolete = 1
                  WHERE name = 'qacontact_accessible'
                        OR name = 'assignee_accessible'");
    }

    # 2005-08-10 Myk Melez <myk@mozilla.org> bug 287325
    # Record each field's type and whether or not it's a custom field,
    # in fielddefs.
    $dbh->bz_add_column('fielddefs', 'type',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
    $dbh->bz_add_column('fielddefs', 'custom',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    $dbh->bz_add_column('fielddefs', 'enter_bug',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    # Change the name of the fieldid column to id, so that fielddefs
    # can use Bugzilla::Object easily. We have to do this up here, because
    # otherwise adding these field definitions will fail.
    $dbh->bz_rename_column('fielddefs', 'fieldid', 'id');

    # If the largest fielddefs sortkey is less than 100, then
    # we're using the old sorting system, and we should convert
    # it to the new one before adding any new definitions.
    if (!$dbh->selectrow_arrayref(
            'SELECT COUNT(id) FROM fielddefs WHERE sortkey >= 100'))
    {
        print "Updating the sortkeys for the fielddefs table...\n";
        my $field_ids = $dbh->selectcol_arrayref(
            'SELECT id FROM fielddefs ORDER BY sortkey');
        my $sortkey = 100;
        foreach my $field_id (@$field_ids) {
            $dbh->do('UPDATE fielddefs SET sortkey = ? WHERE id = ?',
                     undef, $sortkey, $field_id);
            $sortkey += 100;
        }
    }

    $dbh->bz_add_column('fielddefs', 'visibility_field_id', {TYPE => 'INT3'});
    $dbh->bz_add_column('fielddefs', 'value_field_id', {TYPE => 'INT3'});
    $dbh->bz_add_index('fielddefs', 'fielddefs_value_field_id_idx',
                       ['value_field_id']);

    # Bug 344878
    if (!$dbh->bz_column_info('fielddefs', 'buglist')) {
        $dbh->bz_add_column('fielddefs', 'buglist',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
        # Set non-multiselect custom fields as valid buglist fields
        # Note that default fields will be handled in Field.pm
        $dbh->do('UPDATE fielddefs SET buglist = 1 WHERE custom = 1 AND type != ' . FIELD_TYPE_MULTI_SELECT);
    }

    #2008-08-26 elliotte_martin@yahoo.com - Bug 251556
    $dbh->bz_add_column('fielddefs', 'reverse_desc', {TYPE => 'TINYTEXT'});

    $dbh->do('UPDATE fielddefs SET buglist = 1
               WHERE custom = 1 AND type = ' . FIELD_TYPE_MULTI_SELECT);

    $dbh->bz_add_column('fielddefs', 'is_mandatory',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_add_index('fielddefs', 'fielddefs_is_mandatory_idx',
                       ['is_mandatory']);

    # 2010-04-05 dkl@redhat.com - Bug 479400
    _migrate_field_visibility_value();

    $dbh->bz_add_column('fielddefs', 'is_numeric',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->do('UPDATE fielddefs SET is_numeric = 1 WHERE type = '
             . FIELD_TYPE_BUG_ID);

    # 2012-04-12 aliustek@gmail.com - Bug 728138
    $dbh->bz_add_column('fielddefs', 'long_desc',
                        {TYPE => 'varchar(255)', NOTNULL => 1, DEFAULT => "''"}, '');

    Bugzilla::Hook::process('install_update_db_fielddefs');

    # Remember, this is not the function for adding general table changes.
    # That is below. Add new changes to the fielddefs table above this
    # comment.
}

# Small changes can be put directly into this function.
# However, larger changes (more than three or four lines) should
# go into their own private subroutine, and you should call that
# subroutine from this function. That keeps this function readable.
#
# This function runs in historical order--from upgrades that older
# installations need, to upgrades that newer installations need.
# The order of items inside this function should only be changed if 
# absolutely necessary.
#
# The subroutines should have long, descriptive names, so that you
# can easily see what is being done, just by reading this function.
#
# This function is mostly self-documenting. If you're curious about
# what each of the added/removed columns does, you should see the schema 
# docs at:
# http://www.ravenbrook.com/project/p4dti/tool/cgi/bugzilla-schema/
#
# When you add a change, you should only add a comment if you want
# to describe why the change was made. You don't need to describe
# the purpose of a column.
#
sub update_table_definitions {
    my $old_params = shift;
    my $dbh = Bugzilla->dbh;
    _update_pre_checksetup_bugzillas();

    $dbh->bz_add_column('attachments', 'submitter_id',
                        {TYPE => 'INT3', NOTNULL => 1}, 0); 

    $dbh->bz_rename_column('bugs_activity', 'when', 'bug_when');

    _add_bug_vote_cache();
    _update_product_name_definition();

    $dbh->bz_add_column('profiles', 'disabledtext',
                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

    _populate_longdescs();
    _update_bugs_activity_field_to_fieldid();

    if (!$dbh->bz_column_info('bugs', 'lastdiffed')) {
        $dbh->bz_add_column('bugs', 'lastdiffed', {TYPE =>'DATETIME'});
        $dbh->do('UPDATE bugs SET lastdiffed = NOW()');
    }

    _add_unique_login_name_index_to_profiles();

    $dbh->bz_add_column('profiles', 'mybugslink', 
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    _update_component_user_fields_to_ids();

    $dbh->bz_add_column('bugs', 'everconfirmed',
                        {TYPE => 'BOOLEAN', NOTNULL => 1}, 1);

    _populate_milestones_table();

    _add_products_defaultmilestone();

    # 2000-03-24 Added unique indexes into the cc and keyword tables.  This
    # prevents certain database inconsistencies, and, moreover, is required for
    # new generalized list code to work.
    if (!$dbh->bz_index_info('cc', 'cc_bug_id_idx')
        || !$dbh->bz_index_info('cc', 'cc_bug_id_idx')->{TYPE})
    {
        $dbh->bz_drop_index('cc', 'cc_bug_id_idx');
        $dbh->bz_add_index('cc', 'cc_bug_id_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(bug_id who)]});
    }
    if (!$dbh->bz_index_info('keywords', 'keywords_bug_id_idx')
        || !$dbh->bz_index_info('keywords', 'keywords_bug_id_idx')->{TYPE})
    {
        $dbh->bz_drop_index('keywords', 'keywords_bug_id_idx');
        $dbh->bz_add_index('keywords', 'keywords_bug_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(bug_id keywordid)]});
    }

    _copy_from_comments_to_longdescs();
    _populate_duplicates_table();

    if (!$dbh->bz_column_info('email_setting', 'user_id')) {
        $dbh->bz_add_column('profiles', 'emailflags', {TYPE => 'MEDIUMTEXT'});
    }

    $dbh->bz_add_column('groups', 'isactive',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    $dbh->bz_add_column('attachments', 'isobsolete',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    $dbh->bz_drop_column("profiles", "emailnotification");
    $dbh->bz_drop_column("profiles", "newemailtech");

    # 2003-11-19; chicks@chicks.net; bug 225973: fix field size to accommodate
    # wider algorithms such as Blowfish. Note that this needs to be run
    # before recrypting passwords in the following block.
    $dbh->bz_alter_column('profiles', 'cryptpassword',
                          {TYPE => 'varchar(128)'});

    _recrypt_plaintext_passwords();

    # 2001-06-15 kiko@async.com.br - Change bug:version size to avoid
    # truncates re http://bugzilla.mozilla.org/show_bug.cgi?id=9352
    $dbh->bz_alter_column('bugs', 'version',
                          {TYPE => 'varchar(64)', NOTNULL => 1});

    _update_bugs_activity_to_only_record_changes();

    # bug 90933: Make disabledtext NOT NULL
    if (!$dbh->bz_column_info('profiles', 'disabledtext')->{NOTNULL}) {
        $dbh->bz_alter_column("profiles", "disabledtext",
                              {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');
    }

    $dbh->bz_add_column("bugs", "reporter_accessible",
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
    $dbh->bz_add_column("bugs", "cclist_accessible",
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    $dbh->bz_add_column("bugs_activity", "attach_id", {TYPE => 'INT3'});

    _delete_logincookies_cryptpassword_and_handle_invalid_cookies();

    # qacontact/assignee should always be able to see bugs: bug 97471
    $dbh->bz_drop_column("bugs", "qacontact_accessible");
    $dbh->bz_drop_column("bugs", "assignee_accessible");

    # 2002-02-20 jeff.hedlund@matrixsi.com - bug 24789 time tracking
    $dbh->bz_add_column("longdescs", "work_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "estimated_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "remaining_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "deadline", {TYPE => 'DATETIME'});

    _use_ip_instead_of_hostname_in_logincookies();

    $dbh->bz_add_column('longdescs', 'isprivate',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_add_column('attachments', 'isprivate',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    _move_quips_into_db();

    $dbh->bz_drop_column("namedqueries", "watchfordiffs");

    _use_ids_for_products_and_components();
    _convert_groups_system_from_groupset();
    _convert_attachment_statuses_to_flags();
    _remove_spaces_and_commas_from_flagtypes();
    _setup_usebuggroups_backward_compatibility();
    _remove_user_series_map();

    # 2006-08-03 remi_zara@mac.com bug 346241, make series.creator nullable
    # This must happen before calling _copy_old_charts_into_database().
    if ($dbh->bz_column_info('series', 'creator')->{NOTNULL}) {
        $dbh->bz_alter_column('series', 'creator', {TYPE => 'INT3'});
        $dbh->do("UPDATE series SET creator = NULL WHERE creator = 0");
    }

    _copy_old_charts_into_database();

    _add_user_group_map_grant_type();
    _add_group_group_map_grant_type();

    $dbh->bz_add_column("profiles", "extern_id", {TYPE => 'varchar(64)'});

    $dbh->bz_add_column('flagtypes', 'grant_group_id', {TYPE => 'INT3'});
    $dbh->bz_add_column('flagtypes', 'request_group_id', {TYPE => 'INT3'});

    # mailto is no longer just userids
    $dbh->bz_rename_column('whine_schedules', 'mailto_userid', 'mailto');
    $dbh->bz_add_column('whine_schedules', 'mailto_type',
        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '0'});

    _add_longdescs_already_wrapped();

    # Moved enum types to separate tables so we need change the old enum 
    # types to standard varchars in the bugs table.
    $dbh->bz_alter_column('bugs', 'bug_status',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    # 2005-03-23 Tomas.Kopal@altap.cz - add default value to resolution,
    # bug 286695
    $dbh->bz_alter_column('bugs', 'resolution',
        {TYPE => 'varchar(64)', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'priority',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_alter_column('bugs', 'bug_severity',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_alter_column('bugs', 'rep_platform',
                          {TYPE => 'varchar(64)', NOTNULL => 1}, '');
    $dbh->bz_alter_column('bugs', 'op_sys',
                          {TYPE => 'varchar(64)', NOTNULL => 1});

    # When migrating quips from the '$datadir/comments' file to the DB,
    # the user ID should be NULL instead of 0 (which is an invalid user ID).
    if ($dbh->bz_column_info('quips', 'userid')->{NOTNULL}) {
        $dbh->bz_alter_column('quips', 'userid', {TYPE => 'INT3'});
        print "Changing owner to NULL for quips where the owner is",
              " unknown...\n";
        $dbh->do('UPDATE quips SET userid = NULL WHERE userid = 0');
    }

    _convert_attachments_filename_from_mediumtext();

    $dbh->bz_add_column('quips', 'approved',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    # 2002-12-20 Bug 180870 - remove manual shadowdb replication code
    $dbh->bz_drop_table("shadowlog");

    _rename_votes_count_and_force_group_refresh();

    # 2004/02/15 - Summaries shouldn't be null - see bug 220232
    if (!exists $dbh->bz_column_info('bugs', 'short_desc')->{NOTNULL}) {
        $dbh->bz_alter_column('bugs', 'short_desc',
                              {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');
    }

    $dbh->bz_add_column('products', 'classification_id',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

    _fix_group_with_empty_name();

    $dbh->bz_add_index('bugs_activity', 'bugs_activity_who_idx', [qw(who)]);

    # Add defaults for some fields that should have them but didn't.
    $dbh->bz_alter_column('bugs', 'status_whiteboard',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    if ($dbh->bz_column_info('bugs', 'votes')) {
        $dbh->bz_alter_column('bugs', 'votes',
                              {TYPE => 'INT3', NOTNULL => 1, DEFAULT => '0'});
    }

    $dbh->bz_alter_column('bugs', 'lastdiffed', {TYPE => 'DATETIME'});

    # 2005-03-09 qa_contact should be NULL instead of 0, bug 285534
    if ($dbh->bz_column_info('bugs', 'qa_contact')->{NOTNULL}) {
        $dbh->bz_alter_column('bugs', 'qa_contact', {TYPE => 'INT3'});
        $dbh->do("UPDATE bugs SET qa_contact = NULL WHERE qa_contact = 0");
    }

    # 2005-03-27 initialqacontact should be NULL instead of 0, bug 287483
    if ($dbh->bz_column_info('components', 'initialqacontact')->{NOTNULL}) {
        $dbh->bz_alter_column('components', 'initialqacontact', 
                              {TYPE => 'INT3'});
    }
    $dbh->do("UPDATE components SET initialqacontact = NULL " .
              "WHERE initialqacontact = 0");

    _migrate_email_prefs_to_new_table();
    _initialize_new_email_prefs();
    _change_all_mysql_booleans_to_tinyint();

    # make classification_id field type be consistent with DB:Schema
    $dbh->bz_alter_column('products', 'classification_id',
                          {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

    # initialowner was accidentally NULL when we checked-in Schema,
    # when it really should be NOT NULL.
    $dbh->bz_alter_column('components', 'initialowner',
                          {TYPE => 'INT3', NOTNULL => 1}, 0);

    # 2005-03-28 - bug 238800 - index flags.type_id for editflagtypes.cgi
    $dbh->bz_add_index('flags', 'flags_type_id_idx', [qw(type_id)]);

    # For a short time, the flags_type_id_idx was misnamed in upgraded installs.
    $dbh->bz_drop_index('flags', 'type_id');

    # 2005-04-28 - LpSolit@gmail.com - Bug 7233: add an index to versions
    $dbh->bz_alter_column('versions', 'value',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    _add_versions_product_id_index();

    if (!exists $dbh->bz_column_info('milestones', 'sortkey')->{DEFAULT}) {
        $dbh->bz_alter_column('milestones', 'sortkey',
                              {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
    }

    # 2005-06-14 - LpSolit@gmail.com - Bug 292544
    $dbh->bz_alter_column('bugs', 'creation_ts', {TYPE => 'DATETIME'});

    _fix_whine_queries_title_and_op_sys_value();
    _fix_attachments_submitter_id_idx();
    _copy_attachments_thedata_to_attach_data();
    _fix_broken_all_closed_series();
    # 2005-08-14 bugreport@peshkin.net -- Bug 304583
    # Get rid of leftover DERIVED group permissions
    use constant GRANT_DERIVED => 1;
    $dbh->do("DELETE FROM user_group_map WHERE grant_type = " . GRANT_DERIVED);

    _rederive_regex_groups();

    # PUBLIC is a reserved word in Oracle.
    $dbh->bz_rename_column('series', 'public', 'is_public');

    # 2005-11-04 LpSolit@gmail.com - Bug 305927
    $dbh->bz_alter_column('groups', 'userregexp',
                          {TYPE => 'TINYTEXT', NOTNULL => 1, DEFAULT => "''"});

    # 2005-09-26 - olav@bkor.dhs.org - Bug 119524
    $dbh->bz_alter_column('logincookies', 'cookie',
        {TYPE => 'varchar(16)', PRIMARYKEY => 1, NOTNULL => 1}); 

    _clean_control_characters_from_short_desc();
    
    # 2005-12-07 altlst@sonic.net -- Bug 225221
    $dbh->bz_add_column('longdescs', 'comment_id',
        {TYPE => 'INTSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    _stop_storing_inactive_flags();
    _change_short_desc_from_mediumtext_to_varchar();

    # 2006-07-01 wurblzap@gmail.com -- Bug 69000
    $dbh->bz_add_column('namedqueries', 'id',
        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
    _move_namedqueries_linkinfooter_to_its_own_table();

    _add_classifications_sortkey();
    _move_data_nomail_into_db();

    # The products table lacked sensible defaults.
    if ($dbh->bz_column_info('products', 'milestoneurl')) {
        $dbh->bz_alter_column('products', 'milestoneurl',
            {TYPE => 'TINYTEXT', NOTNULL => 1, DEFAULT => "''"});
    }
    if ($dbh->bz_column_info('products', 'disallownew')){
        $dbh->bz_alter_column('products', 'disallownew',
                              {TYPE => 'BOOLEAN', NOTNULL => 1,  DEFAULT => 0});
    
        if ($dbh->bz_column_info('products', 'votesperuser')) {
            $dbh->bz_alter_column('products', 'votesperuser', 
                {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
            $dbh->bz_alter_column('products', 'votestoconfirm',
                {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
        }
    }

    # 2006-08-04 LpSolit@gmail.com - Bug 305941
    $dbh->bz_drop_column('profiles', 'refreshed_when');
    $dbh->bz_drop_column('groups', 'last_changed');

    # 2006-08-06 LpSolit@gmail.com - Bug 347521
    $dbh->bz_alter_column('flagtypes', 'id',
          {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    $dbh->bz_alter_column('keyworddefs', 'id',
        {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    # 2006-08-19 LpSolit@gmail.com - Bug 87795
    $dbh->bz_alter_column('tokens', 'userid', {TYPE => 'INT3'});

    $dbh->bz_drop_index('bugs', 'bugs_short_desc_idx');

    # The profiles table was missing some defaults.
    $dbh->bz_alter_column('profiles', 'disabledtext',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('profiles', 'realname',
        {TYPE => 'varchar(255)', NOTNULL => 1, DEFAULT => "''"});

    _update_longdescs_who_index();

    $dbh->bz_add_column('setting', 'subclass', {TYPE => 'varchar(32)'});

    $dbh->bz_alter_column('longdescs', 'thetext', 
        {TYPE => 'LONGTEXT', NOTNULL => 1}, '');

    # 2006-10-20 LpSolit@gmail.com - Bug 189627
    $dbh->bz_add_column('group_control_map', 'editcomponents',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_add_column('group_control_map', 'editbugs',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_add_column('group_control_map', 'canconfirm',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    # 2006-11-07 LpSolit@gmail.com - Bug 353656
    $dbh->bz_add_column('longdescs', 'type',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column('longdescs', 'extra_data', {TYPE => 'varchar(255)'});

    $dbh->bz_add_column('versions', 'id', 
        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
    $dbh->bz_add_column('milestones', 'id',
        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    _fix_uppercase_custom_field_names();
    _fix_uppercase_index_names();

    # 2007-05-17 LpSolit@gmail.com - Bug 344965
    _initialize_workflow_for_upgrade($old_params);

    # 2007-08-08 LpSolit@gmail.com - Bug 332149
    $dbh->bz_add_column('groups', 'icon_url', {TYPE => 'TINYTEXT'});

    # 2007-08-21 wurblzap@gmail.com - Bug 365378
    _make_lang_setting_dynamic();
    
    # 2007-11-29 xiaoou.wu@oracle.com - Bug 153129
    _change_text_types();

    # 2007-09-09 LpSolit@gmail.com - Bug 99215
    _fix_attachment_modification_date();

    $dbh->bz_drop_index('longdescs', 'longdescs_thetext_idx');
    _populate_bugs_fulltext();

    # 2008-01-18 xiaoou.wu@oracle.com - Bug 414292
    $dbh->bz_alter_column('series', 'query',
        { TYPE => 'MEDIUMTEXT', NOTNULL => 1 });

    # Add FK to multi select field tables
    _add_foreign_keys_to_multiselects();

    # 2008-07-28 tfu@redhat.com - Bug 431669
    $dbh->bz_alter_column('group_control_map', 'product_id',
        { TYPE => 'INT2', NOTNULL => 1 });

    # 2008-09-07 LpSolit@gmail.com - Bug 452893
    _fix_illegal_flag_modification_dates();

    _add_visiblity_value_to_value_tables();

    # 2009-03-02 arbingersys@gmail.com - Bug 423613
    _add_extern_id_index();

    # 2009-03-31 LpSolit@gmail.com - Bug 478972
    $dbh->bz_alter_column('group_control_map', 'entry',
                          {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_alter_column('group_control_map', 'canedit',
                          {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    # 2009-01-16 oreomike@gmail.com - Bug 302420
    $dbh->bz_add_column('whine_events', 'mailifnobugs',
        { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
        
    _convert_disallownew_to_isactive();

    $dbh->bz_alter_column('bugs_activity', 'added', 
        { TYPE => 'varchar(255)' });
    $dbh->bz_add_index('bugs_activity', 'bugs_activity_added_idx', ['added']);

    # 2009-09-28 LpSolit@gmail.com - Bug 519032
    $dbh->bz_drop_column('series', 'last_viewed');

    # 2009-09-28 LpSolit@gmail.com - Bug 399073
    _fix_logincookies_ipaddr();

    # 2009-11-01 LpSolit@gmail.com - Bug 525025
    _fix_invalid_custom_field_names();

    _set_attachment_comment_types();

    $dbh->bz_drop_column('products', 'milestoneurl');

    _add_allows_unconfirmed_to_product_table();
    _convert_flagtypes_fks_to_set_null();
    _fix_decimal_types();
    _fix_series_creator_fk();

    # 2009-11-14 dkl@redhat.com - Bug 310450
    $dbh->bz_add_column('bugs_activity', 'comment_id', {TYPE => 'INT4'});

    # 2010-04-07 LpSolit@gmail.com - Bug 69621
    $dbh->bz_drop_column('bugs', 'keywords');
    
    # 2010-05-07 ewong@pw-wspx.org - Bug 463945
    $dbh->bz_alter_column('group_control_map', 'membercontrol',
                          {TYPE => 'INT1', NOTNULL => 1, DEFAULT => CONTROLMAPNA});
    $dbh->bz_alter_column('group_control_map', 'othercontrol',
                          {TYPE => 'INT1', NOTNULL => 1, DEFAULT => CONTROLMAPNA});

    # Add NOT NULL to some columns that need it, and DEFAULT to
    # attachments.ispatch.
    $dbh->bz_alter_column('attachments', 'ispatch', 
        { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_alter_column('keyworddefs', 'description',
                          { TYPE => 'MEDIUMTEXT', NOTNULL => 1 }, '');
    $dbh->bz_alter_column('products', 'description',
                          { TYPE => 'MEDIUMTEXT', NOTNULL => 1 }, '');

    # Change the default of allows_unconfirmed to TRUE as part
    # of the new workflow.
    $dbh->bz_alter_column('products', 'allows_unconfirmed',
        { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE' });

    # 2010-07-18 LpSolit@gmail.com - Bug 119703
    _remove_attachment_isurl();

    # 2009-05-07 ghendricks@novell.com - Bug 77193
    _add_isactive_to_product_fields();

    # 2010-10-09 LpSolit@gmail.com - Bug 505165
    $dbh->bz_alter_column('flags', 'setter_id', {TYPE => 'INT3', NOTNULL => 1});

    # 2010-10-09 LpSolit@gmail.com - Bug 451735
    _fix_series_indexes();

    $dbh->bz_add_column('bug_see_also', 'id',
        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    _rename_tags_to_tag();

    # 2011-01-29 LpSolit@gmail.com - Bug 616185
    _migrate_user_tags();

    _populate_bug_see_also_class();

    # 2011-06-15 dkl@mozilla.com - Bug 658929
    _migrate_disabledtext_boolean();

    # 2011-11-01 glob@mozilla.com - Bug 240437
    $dbh->bz_add_column('profiles', 'last_seen_date', {TYPE => 'DATETIME'});

    # 2011-10-11 miketosh - Bug 690173
    _on_delete_set_null_for_audit_log_userid();
    
    # 2011-11-23 gerv@gerv.net - Bug 705058 - make filenames longer
    $dbh->bz_alter_column('attachments', 'filename', 
                                    { TYPE => 'varchar(255)', NOTNULL => 1 });

    # 2011-11-28 dkl@mozilla.com - Bug 685611
    _fix_notnull_defaults();

    # 2012-02-15 LpSolit@gmail.com - Bug 722113
    if ($dbh->bz_index_info('profile_search', 'profile_search_user_id')) {
        $dbh->bz_drop_index('profile_search', 'profile_search_user_id');
        $dbh->bz_add_index('profile_search', 'profile_search_user_id_idx', [qw(user_id)]);
    }

    # 2012-03-23 LpSolit@gmail.com - Bug 448551
    $dbh->bz_alter_column('bugs', 'target_milestone',
                          {TYPE => 'varchar(64)', NOTNULL => 1, DEFAULT => "'---'"});

    $dbh->bz_alter_column('milestones', 'value', {TYPE => 'varchar(64)', NOTNULL => 1});

    $dbh->bz_alter_column('products', 'defaultmilestone',
                          {TYPE => 'varchar(64)', NOTNULL => 1, DEFAULT => "'---'"});

    # 2012-04-15 Frank@Frank-Becker.de - Bug 740536
    $dbh->bz_add_index('audit_log', 'audit_log_class_idx', ['class', 'at_time']);

    # 2012-06-06 dkl@mozilla.com - Bug 762288
    $dbh->bz_alter_column('bugs_activity', 'removed', 
                          { TYPE => 'varchar(255)' });
    $dbh->bz_add_index('bugs_activity', 'bugs_activity_removed_idx', ['removed']);

    # 2012-06-13 dkl@mozilla.com - Bug 764457
    $dbh->bz_add_column('bugs_activity', 'id', 
                        {TYPE => 'INTSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    # 2012-06-13 dkl@mozilla.com - Bug 764466
    $dbh->bz_add_column('profiles_activity', 'id', 
                        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    # 2012-07-24 dkl@mozilla.com - Bug 776972
    $dbh->bz_alter_column('bugs_activity', 'id', 
                          {TYPE => 'INTSERIAL', NOTNULL => 1, PRIMARYKEY => 1});


    # 2012-07-24 dkl@mozilla.com - Bug 776982
    _fix_longdescs_primary_key();

    # 2012-08-02 dkl@mozilla.com - Bug 756953
    _fix_dependencies_dupes();

    # 2012-08-01 koosha.khajeh@gmail.com - Bug 187753
    _shorten_long_quips();

    # 2012-12-29 reed@reedloden.com - Bug 785283
    _add_password_salt_separator();

    # 2013-01-02 LpSolit@gmail.com - Bug 824361
    _fix_longdescs_indexes();

    # 2013-02-04 dkl@mozilla.com - Bug 824346
    _fix_flagclusions_indexes();

    # 2013-08-26 sgreen@redhat.com - Bug 903895
    _fix_components_primary_key();

    # 2014-06-09 dylan@mozilla.com - Bug 1022923
    $dbh->bz_add_index('bug_user_last_visit',
                       'bug_user_last_visit_last_visit_ts_idx',
                       ['last_visit_ts']);

    # 2014-07-14 sgreen@redhat.com - Bug 726696
    $dbh->bz_alter_column('tokens', 'tokentype',
                          {TYPE => 'varchar(16)', NOTNULL => 1});

    # 2014-07-27 LpSolit@gmail.com - Bug 1044561
    _fix_user_api_keys_indexes();

    # 2014-08-11 sgreen@redhat.com - Bug 1012506
     _update_alias();

    # 2014-11-10 dkl@mozilla.com - Bug 1093928
    $dbh->bz_drop_column('longdescs', 'is_markdown');

    # 2015-12-16 LpSolit@gmail.com - Bug 1232578
    _sanitize_audit_log_table();

    ################################################################
    # New --TABLE-- changes should go *** A B O V E *** this point #
    ################################################################

    Bugzilla::Hook::process('install_update_db');

    # We do this here because otherwise the foreign key from 
    # products.classification_id to classifications.id will fail
    # (because products.classification_id defaults to "1", so on upgraded
    # installations it's already been set before the first Classification
    # exists).
    Bugzilla::Install::create_default_classification();

    $dbh->bz_setup_foreign_keys();
}

# Subroutines should be ordered in the order that they are called.
# Thus, newer subroutines should be at the bottom.

sub _update_pre_checksetup_bugzillas {
    my $dbh = Bugzilla->dbh;
    # really old fields that were added before checksetup.pl existed
    # but aren't in very old bugzilla's (like 2.1)
    # Steve Stock (sstock@iconnect-inc.com)

    $dbh->bz_add_column('bugs', 'target_milestone',
        {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
    $dbh->bz_add_column('bugs', 'qa_contact', {TYPE => 'INT3'});
    $dbh->bz_add_column('bugs', 'status_whiteboard',
                       {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    if (!$dbh->bz_column_info('products', 'isactive')){
        $dbh->bz_add_column('products', 'disallownew',
                            {TYPE => 'BOOLEAN', NOTNULL => 1}, 0);
    }

    $dbh->bz_add_column('components', 'initialqacontact',
                        {TYPE => 'TINYTEXT'});
    $dbh->bz_add_column('components', 'description',
                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');
}

sub _add_bug_vote_cache {
    my $dbh = Bugzilla->dbh;
    # 1999-10-11 Restructured voting database to add a cached value in each 
    # bug recording how many total votes that bug has.  While I'm at it, 
    # I removed the unused "area" field from the bugs database.  It is 
    # distressing to realize that the bugs table has reached the maximum 
    # number of indices allowed by MySQL (16), which may make future 
    # enhancements awkward.
    # (P.S. All is not lost; it appears that the latest betas of MySQL 
    # support a new table format which will allow 32 indices.)

    if ($dbh->bz_column_info('bugs', 'area')) {
        $dbh->bz_drop_column('bugs', 'area');
        $dbh->bz_add_column('bugs', 'votes', {TYPE => 'INT3', NOTNULL => 1,
                                              DEFAULT => 0});
        $dbh->bz_add_index('bugs', 'bugs_votes_idx', [qw(votes)]);
        $dbh->bz_add_column('products', 'votesperuser',
                            {TYPE => 'INT2', NOTNULL => 1}, 0);
    }
}

sub _update_product_name_definition {
    my $dbh = Bugzilla->dbh;
    # The product name used to be very different in various tables.
    #
    # It was   varchar(16)   in bugs
    #          tinytext      in components
    #          tinytext      in products
    #          tinytext      in versions
    #
    # tinytext is equivalent to varchar(255), which is quite huge, so I change
    # them all to varchar(64).

    # Only do this if these fields still exist - they're removed in
    # a later change
    if ($dbh->bz_column_info('products', 'product')) {
        $dbh->bz_alter_column('bugs',       'product',
                             {TYPE => 'varchar(64)', NOTNULL => 1});
        $dbh->bz_alter_column('components', 'program', {TYPE => 'varchar(64)'});
        $dbh->bz_alter_column('products',   'product', {TYPE => 'varchar(64)'});
        $dbh->bz_alter_column('versions',   'program',
                              {TYPE => 'varchar(64)', NOTNULL => 1});
    }
}

# A helper for the function below.
sub _write_one_longdesc {
    my ($id, $who, $when, $buffer) = (@_);
    my $dbh = Bugzilla->dbh;
    $buffer = trim($buffer);
    return if !$buffer;
    $dbh->do("INSERT INTO longdescs (bug_id, who, bug_when, thetext)
                   VALUES (?,?,?,?)", undef, $id, $who, 
             time2str("%Y/%m/%d %H:%M:%S", $when), $buffer);
}

sub _populate_longdescs {
    my $dbh = Bugzilla->dbh;
    # 2000-01-20 Added a new "longdescs" table, which is supposed to have 
    # all the long descriptions in it, replacing the old long_desc field 
    # in the bugs table. The below hideous code populates this new table 
    # with things from the old field, with ugly parsing and heuristics.

    if ($dbh->bz_column_info('bugs', 'long_desc')) {
        my ($total) = $dbh->selectrow_array("SELECT COUNT(*) FROM bugs");

        print "Populating new long_desc table. This is slow. There are",
              " $total\nbugs to process; a line of dots will be printed",
              " for each 50.\n\n";
        local $| = 1;

        # On MySQL, longdescs doesn't benefit from transactions, but this
        # doesn't hurt.
        $dbh->bz_start_transaction();

        $dbh->do('DELETE FROM longdescs');

        my $sth = $dbh->prepare("SELECT bug_id, creation_ts, reporter,
                                        long_desc FROM bugs ORDER BY bug_id");
        $sth->execute();
        my $count = 0;
        while (my ($id, $createtime, $reporterid, $desc) = 
                   $sth->fetchrow_array()) 
        {
            $count++;
            indicate_progress({ total => $total, current => $count });
            $desc =~ s/\r//g;
            my $who = $reporterid;
            my $when = str2time($createtime);
            my $buffer = "";
            foreach my $line (split(/\n/, $desc)) {
                $line =~ s/\s+$//g; # Trim trailing whitespace.
                if ($line =~ /^------- Additional Comments From ([^\s]+)\s+(\d.+\d)\s+-------$/) 
                {
                    my $name = $1;
                    my $date = str2time($2);
                    # Oy, what a hack.  The creation time is accurate to the
                    # second. But the long text only contains things accurate
                    # to the And so, if someone makes a comment within a 
                    # minute of the original bug creation, then the comment can
                    # come *before* the bug creation.  So, we add 59 seconds to
                    # the time of all comments, so that they are always 
                    # considered to have happened at the *end* of the given
                    # minute, not the beginning.
                    $date += 59;
                    if ($date >= $when) {
                        _write_one_longdesc($id, $who, $when, $buffer);
                        $buffer = "";
                        $when = $date;
                        my $s2 = $dbh->prepare("SELECT userid FROM profiles " .
                                                "WHERE login_name = ?");
                        $s2->execute($name);
                        ($who) = ($s2->fetchrow_array());

                        if (!$who) {
                            # This username doesn't exist.  Maybe someone
                            # renamed them or something.  Invent a new profile
                            # entry disabled, just to represent them.
                            $dbh->do("INSERT INTO profiles (login_name, 
                                      cryptpassword, disabledtext) 
                                      VALUES (?,?,?)", undef, $name, '*',
                                      "Account created only to maintain"
                                      . " database integrity");
                            $who = $dbh->bz_last_key('profiles', 'userid');
                        }
                        next;
                    }
                }
                $buffer .= $line . "\n";
            }
            _write_one_longdesc($id, $who, $when, $buffer);
        } # while loop

        print "\n\n";
        $dbh->bz_drop_column('bugs', 'long_desc');
        $dbh->bz_commit_transaction();
    } # main if
}

sub _update_bugs_activity_field_to_fieldid {
    my $dbh = Bugzilla->dbh;

    # 2000-01-18 Added a new table fielddefs that records information about the
    # different fields we keep an activity log on.  The bugs_activity table
    # now has a pointer into that table instead of recording the name directly.
    if ($dbh->bz_column_info('bugs_activity', 'field')) {
        $dbh->bz_add_column('bugs_activity', 'fieldid',
                            {TYPE => 'INT3', NOTNULL => 1}, 0);

        $dbh->bz_add_index('bugs_activity', 'bugs_activity_fieldid_idx',
                           [qw(fieldid)]);
        print "Populating new bugs_activity.fieldid field...\n";

        $dbh->bz_start_transaction();

        my $ids = $dbh->selectall_arrayref(
            'SELECT DISTINCT fielddefs.id, bugs_activity.field
               FROM bugs_activity LEFT JOIN fielddefs 
                    ON bugs_activity.field = fielddefs.name', {Slice=>{}});

        foreach my $item (@$ids) {
            my $id    = $item->{id};
            my $field = $item->{field};
            # If the id is NULL
            if (!$id) {
                $dbh->do("INSERT INTO fielddefs (name, description) VALUES " .
                         "(?, ?)", undef, $field, $field);
                $id = $dbh->bz_last_key('fielddefs', 'id');
            }
            $dbh->do("UPDATE bugs_activity SET fieldid = ? WHERE field = ?",
                     undef, $id, $field);
        }
        $dbh->bz_commit_transaction();

        $dbh->bz_drop_column('bugs_activity', 'field');
    }
}

sub _add_unique_login_name_index_to_profiles {
    my $dbh = Bugzilla->dbh;

    # 2000-01-22 The "login_name" field in the "profiles" table was not
    # declared to be unique.  Sure enough, somehow, I got 22 duplicated entries
    # in my database.  This code detects that, cleans up the duplicates, and
    # then tweaks the table to declare the field to be unique.  What a pain.
    if (!$dbh->bz_index_info('profiles', 'profiles_login_name_idx')
        || !$dbh->bz_index_info('profiles', 'profiles_login_name_idx')->{TYPE})
    {
        print "Searching for duplicate entries in the profiles table...\n";
        while (1) {
            # This code is weird in that it loops around and keeps doing this
            # select again.  That's because I'm paranoid about deleting entries
            # out from under us in the profiles table.  Things get weird if
            # there are *three* or more entries for the same user...
            my $sth = $dbh->prepare("SELECT p1.userid, p2.userid, p1.login_name
                                       FROM profiles AS p1, profiles AS p2
                                      WHERE p1.userid < p2.userid
                                            AND p1.login_name = p2.login_name
                                   ORDER BY p1.login_name");
            $sth->execute();
            my ($u1, $u2, $n) = ($sth->fetchrow_array);
            last if !$u1;

            print "Both $u1 & $u2 are ids for $n!  Merging $u2 into $u1...\n";
            foreach my $i (["bugs", "reporter"],
                           ["bugs", "assigned_to"],
                           ["bugs", "qa_contact"],
                           ["attachments", "submitter_id"],
                           ["bugs_activity", "who"],
                           ["cc", "who"],
                           ["votes", "who"],
                           ["longdescs", "who"]) {
                my ($table, $field) = (@$i);
                if ($dbh->bz_table_info($table)) {
                    print "   Updating $table.$field...\n";
                    $dbh->do("UPDATE $table SET $field = $u1 " .
                              "WHERE $field = $u2");
                }
            }
            $dbh->do("DELETE FROM profiles WHERE userid = $u2");
        }
        print "OK, changing index type to prevent duplicates in the",
              " future...\n";

        $dbh->bz_drop_index('profiles', 'profiles_login_name_idx');
        $dbh->bz_add_index('profiles', 'profiles_login_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(login_name)]});
    }
}

sub _update_component_user_fields_to_ids {
    my $dbh = Bugzilla->dbh;

    # components.initialowner
    my $comp_init_owner = $dbh->bz_column_info('components', 'initialowner');
    if ($comp_init_owner && $comp_init_owner->{TYPE} eq 'TINYTEXT') {
        my $sth = $dbh->prepare("SELECT program, value, initialowner
                                   FROM components");
        $sth->execute();
        while (my ($program, $value, $initialowner) = $sth->fetchrow_array()) {
            my ($id) = $dbh->selectrow_array(
                "SELECT userid FROM profiles WHERE login_name = ?",
                undef, $initialowner);

            unless (defined $id) {
                print "Warning: You have an invalid default assignee",
                      " '$initialowner'\n in component '$value' of program",
                      " '$program'!\n";
                $id = 0;
            }

            $dbh->do("UPDATE components SET initialowner = ?
                       WHERE program = ? AND value = ?", undef,
                     $id, $program, $value);
        }
        $dbh->bz_alter_column('components','initialowner',{TYPE => 'INT3'});
    }

    # components.initialqacontact
    my $comp_init_qa = $dbh->bz_column_info('components', 'initialqacontact');
    if ($comp_init_qa && $comp_init_qa->{TYPE} eq 'TINYTEXT') {
        my $sth = $dbh->prepare("SELECT program, value, initialqacontact
                                   FROM components");
        $sth->execute();
        while (my ($program, $value, $initialqacontact) = 
                   $sth->fetchrow_array()) 
        {
            my ($id) = $dbh->selectrow_array(
                "SELECT userid FROM profiles WHERE login_name = ?",
                undef, $initialqacontact);

            unless (defined $id) {
                if ($initialqacontact) {
                    print "Warning: You have an invalid default QA contact",
                          " $initialqacontact' in program '$program',",
                          " component '$value'!\n";
                }
                $id = 0;
            }

            $dbh->do("UPDATE components SET initialqacontact = ?
                       WHERE program = ? AND value = ?", undef,
                     $id, $program, $value);
        }

        $dbh->bz_alter_column('components','initialqacontact',{TYPE => 'INT3'});
    }
}

sub _populate_milestones_table {
    my $dbh = Bugzilla->dbh;
    # 2000-03-21 Adding a table for target milestones to
    # database - matthew@zeroknowledge.com
    # If the milestones table is empty, and we're still back in a Bugzilla
    # that has a bugs.product field, that means that we just created
    # the milestones table and it needs to be populated.
    my $milestones_exist = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM milestones");
    if (!$milestones_exist && $dbh->bz_column_info('bugs', 'product')) {
        print "Replacing blank milestones...\n";

        $dbh->do("UPDATE bugs
                     SET target_milestone = '---'
                   WHERE target_milestone = ' '");

        # If we are upgrading from 2.8 or earlier, we will have *created*
        # the milestones table with a product_id field, but Bugzilla expects
        # it to have a "product" field. So we change the field backward so
        # other code can run. The change will be reversed later in checksetup.
        if ($dbh->bz_column_info('milestones', 'product_id')) {
            # Dropping the column leaves us with a milestones_product_id_idx
            # index that is only on the "value" column. We need to drop the
            # whole index so that it can be correctly re-created later.
            $dbh->bz_drop_index('milestones', 'milestones_product_id_idx');
            $dbh->bz_drop_column('milestones', 'product_id');
            $dbh->bz_add_column('milestones', 'product',
                                {TYPE => 'varchar(64)', NOTNULL => 1}, '');
        }

        # Populate the milestone table with all existing values in the database
        my $sth = $dbh->prepare("SELECT DISTINCT target_milestone, product 
                                   FROM bugs");
        $sth->execute();

        print "Populating milestones table...\n";

        while (my ($value, $product) = $sth->fetchrow_array()) {
            # check if the value already exists
            my $sortkey = substr($value, 1);
            if ($sortkey !~ /^\d+$/) {
                $sortkey = 0;
            } else {
                $sortkey *= 10;
            }
            my $ms_exists = $dbh->selectrow_array(
                "SELECT value FROM milestones
                  WHERE value = ? AND product = ?", undef, $value, $product);

            if (!$ms_exists) {
                $dbh->do("INSERT INTO milestones(value, product, sortkey) 
                          VALUES (?,?,?)", undef, $value, $product, $sortkey);
            }
        }
    }
}

sub _add_products_defaultmilestone {
    my $dbh = Bugzilla->dbh;

    # 2000-03-23 Added a defaultmilestone field to the products table, so that
    # we know which milestone to initially assign bugs to.
    if (!$dbh->bz_column_info('products', 'defaultmilestone')) {
        $dbh->bz_add_column('products', 'defaultmilestone',
                 {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
        my $sth = $dbh->prepare(
            "SELECT product, defaultmilestone FROM products");
        $sth->execute();
        while (my ($product, $default_ms) = $sth->fetchrow_array()) {
            my $exists = $dbh->selectrow_array(
                "SELECT value FROM milestones
                  WHERE value = ? AND product = ?", 
                undef, $default_ms, $product);
            if (!$exists) {
                $dbh->do("INSERT INTO milestones(value, product) " .
                         "VALUES (?, ?)", undef, $default_ms, $product);
            }
        }
    }
}

sub _copy_from_comments_to_longdescs {
    my $dbh = Bugzilla->dbh;
    # 2000-11-27 For Bugzilla 2.5 and later. Copy data from 'comments' to
    # 'longdescs' - the new name of the comments table.
    if ($dbh->bz_table_info('comments')) {
        print "Copying data from 'comments' to 'longdescs'...\n";
        my $quoted_when = $dbh->quote_identifier('when');
        $dbh->do("INSERT INTO longdescs (bug_when, bug_id, who, thetext)
                  SELECT $quoted_when, bug_id, who, comment
                    FROM comments");
        $dbh->bz_drop_table("comments");
    }
}

sub _populate_duplicates_table {
    my $dbh = Bugzilla->dbh;
    # 2000-07-15 Added duplicates table so Bugzilla tracks duplicates in a
    # better way than it used to. This code searches the comments to populate
    # the table initially. It's executed if the table is empty; if it's 
    # empty because there are no dupes (as opposed to having just created 
    # the table) it won't have any effect anyway, so it doesn't matter.
    my ($dups_exist) = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM duplicates");
    # We also check against a schema change that happened later.
    if (!$dups_exist && !$dbh->bz_column_info('groups', 'isactive')) {
        # populate table
        print "Populating duplicates table from comments...\n";

        my $sth = $dbh->prepare(
             "SELECT longdescs.bug_id, thetext
                FROM longdescs LEFT JOIN bugs 
                     ON longdescs.bug_id = bugs.bug_id
               WHERE (" . $dbh->sql_regexp("thetext", 
                          "'[.*.]{3} This bug has been marked as a duplicate"
                          . " of [[:digit:]]+ [.*.]{3}'") 
                  . ")
                     AND resolution = 'DUPLICATE'
            ORDER BY longdescs.bug_when");
        $sth->execute();

        my (%dupes, $key);
        # Because of the way hashes work, this loop removes all but the 
        # last dupe resolution found for a given bug.
        while (my ($dupe, $dupe_of) = $sth->fetchrow_array()) {
            $dupes{$dupe} = $dupe_of;
        }

        foreach $key (keys(%dupes)){
            $dupes{$key} =~ /^.*\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*$/ms;
            $dupes{$key} = $1;
            $dbh->do("INSERT INTO duplicates VALUES(?, ?)", undef,
                     $dupes{$key},  $key);
            #        BugItsADupeOf  Dupe
        }
    }
}

sub _recrypt_plaintext_passwords {
    my $dbh = Bugzilla->dbh;
    # 2001-06-12; myk@mozilla.org; bugs 74032, 77473:
    # Recrypt passwords using Perl &crypt instead of the mysql equivalent
    # and delete plaintext passwords from the database.
    if ($dbh->bz_column_info('profiles', 'password')) {

        print <<ENDTEXT;
Your current installation of Bugzilla stores passwords in plaintext
in the database and uses mysql's encrypt function instead of Perl's
crypt function to crypt passwords.  Passwords are now going to be
re-crypted with the Perl function, and plaintext passwords will be
deleted from the database.  This could take a while if your
installation has many users.
ENDTEXT

        # Re-crypt everyone's password.
        my $total = $dbh->selectrow_array('SELECT COUNT(*) FROM profiles');
        my $sth = $dbh->prepare("SELECT userid, password FROM profiles");
        $sth->execute();

        my $i = 1;

        print "Fixing passwords...\n";
        while (my ($userid, $password) = $sth->fetchrow_array()) {
            my $cryptpassword = $dbh->quote(bz_crypt($password));
            $dbh->do("UPDATE profiles " .
                        "SET cryptpassword = $cryptpassword " .
                      "WHERE userid = $userid");
            indicate_progress({ total => $total, current => $i, every => 10 });
        }
        print "\n";

        # Drop the plaintext password field.
        $dbh->bz_drop_column('profiles', 'password');
    }
}

sub _update_bugs_activity_to_only_record_changes {
    my $dbh = Bugzilla->dbh;
    # 2001-07-20 jake@bugzilla.org - Change bugs_activity to only record changes
    #  http://bugzilla.mozilla.org/show_bug.cgi?id=55161
    if ($dbh->bz_column_info('bugs_activity', 'oldvalue')) {
        $dbh->bz_add_column("bugs_activity", "removed", {TYPE => "TINYTEXT"});
        $dbh->bz_add_column("bugs_activity", "added", {TYPE => "TINYTEXT"});

        # Need to get field id's for the fields that have multiple values
        my @multi;
        foreach my $f ("cc", "dependson", "blocked", "keywords") {
            my $sth = $dbh->prepare("SELECT id " .
                                    "FROM fielddefs " .
                                    "WHERE name = '$f'");
            $sth->execute();
            my ($fid) = $sth->fetchrow_array();
            push (@multi, $fid);
        }

        # Now we need to process the bugs_activity table and reformat the data
        print "Fixing activity log...\n";
        my $total = $dbh->selectrow_array('SELECT COUNT(*) FROM bugs_activity');
        my $sth = $dbh->prepare("SELECT bug_id, who, bug_when, fieldid,
                                oldvalue, newvalue FROM bugs_activity");
        $sth->execute;
        my $i = 0;
        while (my ($bug_id, $who, $bug_when, $fieldid, $oldvalue, $newvalue)
                   = $sth->fetchrow_array()) 
        {
            $i++;
            indicate_progress({ total => $total, current => $i, every => 10 });
            # Make sure (old|new)value isn't null (to suppress warnings)
            $oldvalue ||= "";
            $newvalue ||= "";
            my ($added, $removed) = "";
            if (grep ($_ eq $fieldid, @multi)) {
                $oldvalue =~ s/[\s,]+/ /g;
                $newvalue =~ s/[\s,]+/ /g;
                my @old = split(" ", $oldvalue);
                my @new = split(" ", $newvalue);
                my (@add, @remove) = ();
                # Find values that were "added"
                foreach my $value(@new) {
                    if (! grep ($_ eq $value, @old)) {
                        push (@add, $value);
                    }
                }
                # Find values that were removed
                foreach my $value(@old) {
                    if (! grep ($_ eq $value, @new)) {
                        push (@remove, $value);
                    }
                }
                $added = join (", ", @add);
                $removed = join (", ", @remove);
                # If we can't determine what changed, put a ? in both fields
                unless ($added || $removed) {
                    $added = "?";
                    $removed = "?";
                }
                # If the original field (old|new)value was full, then this
                # could be incomplete data.
                if (length($oldvalue) == 255 || length($newvalue) == 255) {
                    $added = "? $added";
                    $removed = "? $removed";
                }
            } else {
                $removed = $oldvalue;
                $added = $newvalue;
            }
            $added = $dbh->quote($added);
            $removed = $dbh->quote($removed);
            $dbh->do("UPDATE bugs_activity 
                         SET removed = $removed, added = $added
                       WHERE bug_id = $bug_id AND who = $who
                             AND bug_when = '$bug_when' 
                             AND fieldid = $fieldid");
        }
        print "\n";
        $dbh->bz_drop_column("bugs_activity", "oldvalue");
        $dbh->bz_drop_column("bugs_activity", "newvalue");
    }
}

sub _delete_logincookies_cryptpassword_and_handle_invalid_cookies {
    my $dbh = Bugzilla->dbh;
    # 2002-02-04 bbaetz@student.usyd.edu.au bug 95732
    # Remove logincookies.cryptpassword, and delete entries which become
    # invalid
    if ($dbh->bz_column_info("logincookies", "cryptpassword")) {
        # We need to delete any cookies which are invalid before dropping the
        # column
        print "Removing invalid login cookies...\n";

        # mysql doesn't support DELETE with multi-table queries, so we have
        # to iterate
        my $sth = $dbh->prepare("SELECT cookie FROM logincookies, profiles " .
                                "WHERE logincookies.cryptpassword != " .
                                "profiles.cryptpassword AND " .
                                "logincookies.userid = profiles.userid");
        $sth->execute();
        while (my ($cookie) = $sth->fetchrow_array()) {
            $dbh->do("DELETE FROM logincookies WHERE cookie = $cookie");
        }

        $dbh->bz_drop_column("logincookies", "cryptpassword");
    }
}

sub _use_ip_instead_of_hostname_in_logincookies {
    my $dbh = Bugzilla->dbh;

    # 2002-03-15 bbaetz@student.usyd.edu.au - bug 129466
    # 2002-05-13 preed@sigkill.com - bug 129446 patch backported to the
    #  BUGZILLA-2_14_1-BRANCH as a security blocker for the 2.14.2 release
    #
    # Use the ip, not the hostname, in the logincookies table
    if ($dbh->bz_column_info("logincookies", "hostname")) {
        print "Clearing the logincookies table...\n";
        # We've changed what we match against, so all entries are now invalid
        $dbh->do("DELETE FROM logincookies");

        # Now update the logincookies schema
        $dbh->bz_drop_column("logincookies", "hostname");
        $dbh->bz_add_column("logincookies", "ipaddr",
                            {TYPE => 'varchar(40)'});
    }
}

sub _move_quips_into_db {
    my $dbh = Bugzilla->dbh;
    my $datadir = bz_locations->{'datadir'};
    # 2002-07-15 davef@tetsubo.com - bug 67950
    # Move quips to the db.
    if (-e "$datadir/comments") {
        print "Populating quips table from $datadir/comments...\n";
        my $comments = new IO::File("$datadir/comments", 'r')
            || die "$datadir/comments: $!";
        $comments->untaint;
        while (<$comments>) {
            chomp;
            $dbh->do("INSERT INTO quips (quip) VALUES (?)", undef, $_);
        }

        print "\n", install_string('update_quips', { data => $datadir }), "\n";
        $comments->close;
        rename("$datadir/comments", "$datadir/comments.bak")
            || warn "Failed to rename: $!";
    }
}

sub _use_ids_for_products_and_components {
    my $dbh = Bugzilla->dbh;
    # 2002-08-12 jake@bugzilla.org/bbaetz@student.usyd.edu.au - bug 43600
    # Use integer IDs for products and components.
    if ($dbh->bz_column_info("products", "product")) {
        print "Updating database to use product IDs.\n";

        # First, we need to remove possible NULL entries
        # NULLs may exist, but won't have been used, since all the uses of them
        # are in NOT NULL fields in other tables
        $dbh->do("DELETE FROM products WHERE product IS NULL");
        $dbh->do("DELETE FROM components WHERE value IS NULL");

        $dbh->bz_add_column("products", "id",
            {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
        $dbh->bz_add_column("components", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("versions", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("milestones", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("bugs", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);

        # The attachstatusdefs table was added in version 2.15, but 
        # removed again in early 2.17.  If it exists now, we still need 
        # to perform this change with product_id because the code later on
        # which converts the attachment statuses to flags depends on it.
        # But we need to avoid this if the user is upgrading from 2.14
        # or earlier (because it won't be there to convert).
        if ($dbh->bz_table_info("attachstatusdefs")) {
            $dbh->bz_add_column("attachstatusdefs", "product_id",
                {TYPE => 'INT2', NOTNULL => 1}, 0);
        }

        my %products;
        my $sth = $dbh->prepare("SELECT id, product FROM products");
        $sth->execute;
        while (my ($product_id, $product) = $sth->fetchrow_array()) {
            if (exists $products{$product}) {
                print "Ignoring duplicate product $product\n";
                $dbh->do("DELETE FROM products WHERE id = $product_id");
                next;
            }
            $products{$product} = 1;
            $dbh->do("UPDATE components SET product_id = $product_id " .
                     "WHERE program = " . $dbh->quote($product));
            $dbh->do("UPDATE versions SET product_id = $product_id " .
                     "WHERE program = " . $dbh->quote($product));
            $dbh->do("UPDATE milestones SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product));
            $dbh->do("UPDATE bugs SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product));
            $dbh->do("UPDATE attachstatusdefs SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product))
                if $dbh->bz_table_info("attachstatusdefs");
        }

        print "Updating the database to use component IDs.\n";

        $dbh->bz_add_column("components", "id",
            {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
        $dbh->bz_add_column("bugs", "component_id",
                            {TYPE => 'INT3', NOTNULL => 1}, 0);

        my %components;
        $sth = $dbh->prepare("SELECT id, value, product_id FROM components");
        $sth->execute;
        while (my ($component_id, $component, $product_id) 
            = $sth->fetchrow_array()) 
        {
            if (exists $components{$component}) {
                if (exists $components{$component}{$product_id}) {
                    print "Ignoring duplicate component $component for",
                          " product $product_id\n";
                    $dbh->do("DELETE FROM components WHERE id = $component_id");
                    next;
                }
            } else {
                $components{$component} = {};
            }
            $components{$component}{$product_id} = 1;
            $dbh->do("UPDATE bugs SET component_id = $component_id " .
                      "WHERE component = " . $dbh->quote($component) .
                       " AND product_id = $product_id");
        }
        print "Fixing Indexes and Uniqueness.\n";
        $dbh->bz_drop_index('milestones', 'milestones_product_idx');

        $dbh->bz_add_index('milestones', 'milestones_product_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});

        $dbh->bz_drop_index('bugs', 'bugs_product_idx');
        $dbh->bz_add_index('bugs', 'bugs_product_id_idx', [qw(product_id)]);
        $dbh->bz_drop_index('bugs', 'bugs_component_idx');
        $dbh->bz_add_index('bugs', 'bugs_component_id_idx', [qw(component_id)]);

        print "Removing, renaming, and retyping old product and",
              " component fields.\n";
        $dbh->bz_drop_column("components", "program");
        $dbh->bz_drop_column("versions", "program");
        $dbh->bz_drop_column("milestones", "product");
        $dbh->bz_drop_column("bugs", "product");
        $dbh->bz_drop_column("bugs", "component");
        $dbh->bz_drop_column("attachstatusdefs", "product")
            if $dbh->bz_table_info("attachstatusdefs");
        $dbh->bz_rename_column("products", "product", "name");
        $dbh->bz_alter_column("products", "name",
                              {TYPE => 'varchar(64)', NOTNULL => 1});
        $dbh->bz_rename_column("components", "value", "name");
        $dbh->bz_alter_column("components", "name",
                              {TYPE => 'varchar(64)', NOTNULL => 1});

        print "Adding indexes for products and components tables.\n";
        $dbh->bz_add_index('products', 'products_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(name)]});
        $dbh->bz_add_index('components', 'components_product_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(product_id name)]});
        $dbh->bz_add_index('components', 'components_name_idx', [qw(name)]);
    }
}

# Helper for the below function.
#
# _list_bits(arg) returns a list of UNKNOWN<n> if the group
# has been deleted for all bits set in arg. When the activity
# records are converted from groupset numbers to lists of
# group names, _list_bits is used to fill in a list of references
# to groupset bits for groups that no longer exist.
sub _list_bits {
    my ($num) = @_;
    my $dbh = Bugzilla->dbh;
    my @res;
    my $curr = 1;
    while (1) {
        # Convert a big integer to a list of bits
        my $sth = $dbh->prepare("SELECT ($num & ~$curr) > 0,
                                        ($num & $curr),
                                        ($num & ~$curr),
                                        $curr << 1");
        $sth->execute;
        my ($more, $thisbit, $remain, $nval) = $sth->fetchrow_array;
        push @res,"UNKNOWN<$curr>" if ($thisbit);
        $curr = $nval;
        $num = $remain;
        last if !$more;
    }
    return @res;
}

sub _convert_groups_system_from_groupset {
    my $dbh = Bugzilla->dbh;
    # 2002-09-22 - bugreport@peshkin.net - bug 157756
    #
    # If the whole groups system is new, but the installation isn't,
    # convert all the old groupset groups, etc...
    #
    # This requires:
    # 1) define groups ids in group table
    # 2) populate user_group_map with grants from old groupsets 
    #    and blessgroupsets
    # 3) populate bug_group_map with data converted from old bug groupsets
    # 4) convert activity logs to use group names instead of numbers
    # 5) identify the admin from the old all-ones groupset

    # The groups system needs to be converted if groupset exists
    if ($dbh->bz_column_info("profiles", "groupset")) {
        # Some mysql versions will promote any unique key to primary key
        # so all unique keys are removed first and then added back in
        $dbh->bz_drop_index('groups', 'groups_bit_idx');
        $dbh->bz_drop_index('groups', 'groups_name_idx');
        my @primary_key = $dbh->primary_key(undef, undef, 'groups');
        if (@primary_key) {
            $dbh->do("ALTER TABLE groups DROP PRIMARY KEY");
        }

        $dbh->bz_add_column('groups', 'id',
            {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

        $dbh->bz_add_index('groups', 'groups_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(name)]});

        # Convert all existing groupset records to map entries before removing
        # groupset fields or removing "bit" from groups.
        my $sth = $dbh->prepare("SELECT bit, id FROM groups WHERE bit > 0");
        $sth->execute();
        while (my ($bit, $gid) = $sth->fetchrow_array) {
            # Create user_group_map membership grants for old groupsets.
            # Get each user with the old groupset bit set
            my $sth2 = $dbh->prepare("SELECT userid FROM profiles
                                       WHERE (groupset & $bit) != 0");
            $sth2->execute();
            while (my ($uid) = $sth2->fetchrow_array) {
                # Check to see if the user is already a member of the group
                # and, if not, insert a new record.
                my $query = "SELECT user_id FROM user_group_map
                              WHERE group_id = $gid AND user_id = $uid
                                    AND isbless = 0";
                my $sth3 = $dbh->prepare($query);
                $sth3->execute();
                if ( !$sth3->fetchrow_array() ) {
                    $dbh->do("INSERT INTO user_group_map
                              (user_id, group_id, isbless, grant_type)
                              VALUES ($uid, $gid, 0, " . GRANT_DIRECT . ")");
                }
            }
            # Create user can bless group grants for old groupsets, but only
            # if we're upgrading from a Bugzilla that had blessing.
            if($dbh->bz_column_info('profiles', 'blessgroupset')) {
                # Get each user with the old blessgroupset bit set
                $sth2 = $dbh->prepare("SELECT userid FROM profiles
                                        WHERE (blessgroupset & $bit) != 0");
                $sth2->execute();
                while (my ($uid) = $sth2->fetchrow_array) {
                    $dbh->do("INSERT INTO user_group_map
                              (user_id, group_id, isbless, grant_type)
                              VALUES ($uid, $gid, 1, " . GRANT_DIRECT . ")");
                }
            }
            # Create bug_group_map records for old groupsets.
            # Get each bug with the old group bit set.
            $sth2 = $dbh->prepare("SELECT bug_id FROM bugs
                                    WHERE (groupset & $bit) != 0");
            $sth2->execute();
            while (my ($bug_id) = $sth2->fetchrow_array) {
               # Insert the bug, group pair into the bug_group_map.
                $dbh->do("INSERT INTO bug_group_map (bug_id, group_id)
                          VALUES ($bug_id, $gid)");
            }
        }
        # Replace old activity log groupset records with lists of names 
        # of groups.
        $sth = $dbh->prepare("SELECT id FROM fielddefs
                               WHERE name = " . $dbh->quote('bug_group'));
        $sth->execute();
        my ($bgfid) = $sth->fetchrow_array;
        # Get the field id for the old groupset field
        $sth = $dbh->prepare("SELECT id FROM fielddefs
                               WHERE name = " . $dbh->quote('groupset'));
        $sth->execute();
        my ($gsid) = $sth->fetchrow_array;
        # Get all bugs_activity records from groupset changes
        if ($gsid) {
            $sth = $dbh->prepare("SELECT bug_id, bug_when, who, added, removed
                                    FROM bugs_activity WHERE fieldid = $gsid");
            $sth->execute();
            while (my ($bug_id, $bug_when, $who, $added, $removed) = 
                       $sth->fetchrow_array) 
            {
                $added ||= 0;
                $removed ||= 0;
                # Get names of groups added.
                my $sth2 = $dbh->prepare("SELECT name FROM groups
                                           WHERE (bit & $added) != 0
                                                 AND (bit & $removed) = 0");
                $sth2->execute();
                my @logadd;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logadd, $n;
                }
                # Get names of groups removed.
                $sth2 = $dbh->prepare("SELECT name FROM groups
                                        WHERE (bit & $removed) != 0
                                              AND (bit & $added) = 0");
                $sth2->execute();
                my @logrem;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logrem, $n;
                }
                # Get list of group bits added that correspond to 
                # missing groups.
                $sth2 = $dbh->prepare("SELECT ($added & ~BIT_OR(bit)) 
                                         FROM groups");
                $sth2->execute();
                my ($miss) = $sth2->fetchrow_array;
                if ($miss) {
                    push @logadd, _list_bits($miss);
                    print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id",
                          " CONTAINS DELETED GROUPS\n";
                }
                # Get list of group bits deleted that correspond to 
                # missing groups.
                $sth2 = $dbh->prepare("SELECT ($removed & ~BIT_OR(bit)) 
                                         FROM groups");
                $sth2->execute();
                ($miss) = $sth2->fetchrow_array;
                if ($miss) {
                    push @logrem, _list_bits($miss);
                    print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id",
                          " CONTAINS DELETED GROUPS\n";
                }
                my $logr = "";
                my $loga = "";
                $logr = join(", ", @logrem) . '?' if @logrem;
                $loga = join(", ", @logadd) . '?' if @logadd;
                # Replace to old activity record with the converted data.
                $dbh->do("UPDATE bugs_activity SET fieldid = $bgfid, added = " .
                          $dbh->quote($loga) . ", removed = " .
                          $dbh->quote($logr) .
                         " WHERE bug_id = $bug_id AND bug_when = " .
                          $dbh->quote($bug_when) .
                         " AND who = $who AND fieldid = $gsid");
            }
            # Replace groupset changes with group name changes in
            # profiles_activity. Get profiles_activity records for groupset.
            $sth = $dbh->prepare(
                 "SELECT userid, profiles_when, who, newvalue, oldvalue " .
                   "FROM profiles_activity " .
                  "WHERE fieldid = $gsid");
            $sth->execute();
            while (my ($uid, $uwhen, $uwho, $added, $removed) = 
                       $sth->fetchrow_array) 
            {
                $added ||= 0;
                $removed ||= 0;
                # Get names of groups added.
                my $sth2 = $dbh->prepare("SELECT name FROM groups
                                           WHERE (bit & $added) != 0
                                                 AND (bit & $removed) = 0");
                $sth2->execute();
                my @logadd;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logadd, $n;
                }
                # Get names of groups removed.
                $sth2 = $dbh->prepare("SELECT name FROM groups
                                        WHERE (bit & $removed) != 0
                                              AND (bit & $added) = 0");
                $sth2->execute();
                my @logrem;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logrem, $n;
                }
                my $ladd = "";
                my $lrem = "";
                $ladd = join(", ", @logadd) . '?' if @logadd;
                $lrem = join(", ", @logrem) . '?' if @logrem;
                # Replace profiles_activity record for groupset change
                # with group list.
                $dbh->do("UPDATE profiles_activity " .
                            "SET fieldid = $bgfid, newvalue = " .
                          $dbh->quote($ladd) . ", oldvalue = " .
                          $dbh->quote($lrem) .
                         " WHERE userid = $uid AND profiles_when = " .
                          $dbh->quote($uwhen) .
                               " AND who = $uwho AND fieldid = $gsid");
            }
        }

        # Identify admin group.
        my ($admin_gid) = $dbh->selectrow_array(
            "SELECT id FROM groups WHERE name = 'admin'");
        if (!$admin_gid) {
            $dbh->do(q{INSERT INTO groups (name, description)
                                   VALUES ('admin', 'Administrators')});
            $admin_gid = $dbh->bz_last_key('groups', 'id');
        }
        # Find current admins
        my @admins;
        # Don't lose admins from DBs where Bug 157704 applies
        $sth = $dbh->prepare(
               "SELECT userid, (groupset & 65536), login_name " .
                 "FROM profiles " .
                "WHERE (groupset | 65536) = 9223372036854775807");
        $sth->execute();
        while ( my ($userid, $iscomplete, $login_name) 
                    = $sth->fetchrow_array() ) 
        {
            # existing administrators are made members of group "admin"
            print "\nWARNING - $login_name IS AN ADMIN IN SPITE OF BUG",
                  " 157704\n\n" if (!$iscomplete);
            push(@admins, $userid) unless grep($_ eq $userid, @admins);
        }
        # Now make all those users admins directly. They were already
        # added to every other group, above, because of their groupset.
        foreach my $admin_id (@admins) {
            $dbh->do("INSERT INTO user_group_map
                                  (user_id, group_id, isbless, grant_type)
                           VALUES (?, ?, ?, ?)", 
                      undef, $admin_id, $admin_gid, $_, GRANT_DIRECT)
                foreach (0, 1);
        }

        $dbh->bz_drop_column('profiles','groupset');
        $dbh->bz_drop_column('profiles','blessgroupset');
        $dbh->bz_drop_column('bugs','groupset');
        $dbh->bz_drop_column('groups','bit');
        $dbh->do("DELETE FROM fielddefs WHERE name = " 
            . $dbh->quote('groupset'));
    }
}

sub _convert_attachment_statuses_to_flags {
    my $dbh = Bugzilla->dbh;

    # September 2002 myk@mozilla.org bug 98801
    # Convert the attachment statuses tables into flags tables.
    if ($dbh->bz_table_info("attachstatuses") 
        && $dbh->bz_table_info("attachstatusdefs")) 
    {
        print "Converting attachment statuses to flags...\n";

        # Get IDs for the old attachment status and new flag fields.
        my ($old_field_id) = $dbh->selectrow_array(
            "SELECT id FROM fielddefs WHERE name='attachstatusdefs.name'")
            || 0;
        my ($new_field_id) = $dbh->selectrow_array(
            "SELECT id FROM fielddefs WHERE name = 'flagtypes.name'");

        # Convert attachment status definitions to flag types.  If more than one
        # status has the same name and description, it is merged into a single
        # status with multiple inclusion records.     

        my $sth = $dbh->prepare(
            "SELECT id, name, description, sortkey, product_id  
               FROM attachstatusdefs");

        # status definition IDs indexed by name/description
        my $def_ids = {};

        # merged IDs and the IDs they were merged into.  The key is the old ID,
        # the value is the new one.  This allows us to give statuses the right
        # ID when we convert them over to flags.  This map includes IDs that
        # weren't merged (in this case the old and new IDs are the same), since
        # it makes the code simpler.
        my $def_id_map = {};

        $sth->execute();
        while (my ($id, $name, $desc, $sortkey, $prod_id) = 
                   $sth->fetchrow_array()) 
        {
            my $key = $name . $desc;
            if (!$def_ids->{$key}) {
                $def_ids->{$key} = $id;
                my $quoted_name = $dbh->quote($name);
                my $quoted_desc = $dbh->quote($desc);
                $dbh->do("INSERT INTO flagtypes (id, name, description, 
                                                 sortkey, target_type) 
                          VALUES ($id, $quoted_name, $quoted_desc, 
                                  $sortkey,'a')");
            }
            $def_id_map->{$id} = $def_ids->{$key};
            $dbh->do("INSERT INTO flaginclusions (type_id, product_id)
                      VALUES ($def_id_map->{$id}, $prod_id)");
        }

        # Note: even though we've converted status definitions, we still
        # can't drop the table because we need it to convert the statuses 
        # themselves.

        # Convert attachment statuses to flags.  To do this we select 
        # the statuses from the status table and then, for each one, 
        # figure out who set it and when they set it from the bugs 
        # activity table.
        my $id = 0;
        $sth = $dbh->prepare(
            "SELECT attachstatuses.attach_id, attachstatusdefs.id,
                    attachstatusdefs.name, attachments.bug_id
               FROM attachstatuses, attachstatusdefs, attachments
              WHERE attachstatuses.statusid = attachstatusdefs.id
                    AND attachstatuses.attach_id = attachments.attach_id");

        # a query to determine when the attachment status was set and who set it
        my $sth2 = $dbh->prepare("SELECT added, who, bug_when
                                    FROM bugs_activity
                                   WHERE bug_id = ? AND attach_id = ?
                                         AND fieldid = $old_field_id
                                ORDER BY bug_when DESC");

        $sth->execute();
        while (my ($attach_id, $def_id, $status, $bug_id) = 
                   $sth->fetchrow_array()) 
        {
            ++$id;

            # Determine when the attachment status was set and who set it.
            # We should always be able to find out this info from the bug
            # activity, but we fall back to default values just in case.
            $sth2->execute($bug_id, $attach_id);
            my ($added, $who, $when);
            while (($added, $who, $when) = $sth2->fetchrow_array()) {
                last if $added =~ /(^|[, ]+)\Q$status\E([, ]+|$)/;
            }
            $who = $dbh->quote($who); # "NULL" by default if $who is undefined
            $when = $when ? $dbh->quote($when) : "NOW()";


            $dbh->do("INSERT INTO flags (id, type_id, status, bug_id, 
                      attach_id, creation_date, modification_date, 
                      requestee_id, setter_id)
                      VALUES ($id, $def_id_map->{$def_id}, '+', $bug_id,
                              $attach_id, $when, $when, NULL, $who)");
        }

        # Now that we've converted both tables we can drop them.
        $dbh->bz_drop_table("attachstatuses");
        $dbh->bz_drop_table("attachstatusdefs");

        # Convert activity records for attachment statuses into records 
        # for flags.
        $sth = $dbh->prepare("SELECT attach_id, who, bug_when, added, 
                                     removed 
                                FROM bugs_activity 
                               WHERE fieldid = $old_field_id");
        $sth->execute();
        while (my ($attach_id, $who, $when, $old_added, $old_removed) =
                   $sth->fetchrow_array())
        {
            my @additions = split(/[, ]+/, $old_added);
            @additions = map("$_+", @additions);
            my $new_added = $dbh->quote(join(", ", @additions));

            my @removals = split(/[, ]+/, $old_removed);
            @removals = map("$_+", @removals);
            my $new_removed = $dbh->quote(join(", ", @removals));

            $old_added = $dbh->quote($old_added);
            $old_removed = $dbh->quote($old_removed);
            $who = $dbh->quote($who);
            $when = $dbh->quote($when);

            $dbh->do("UPDATE bugs_activity SET fieldid = $new_field_id, " .
                     "added = $new_added, removed = $new_removed " .
                     "WHERE attach_id = $attach_id AND who = $who " .
                     "AND bug_when = $when AND fieldid = $old_field_id " .
                     "AND added = $old_added AND removed = $old_removed");
        }

        # Remove the attachment status field from the field definitions.
        $dbh->do("DELETE FROM fielddefs WHERE name='attachstatusdefs.name'");

        print "done.\n";
    }
}

sub _remove_spaces_and_commas_from_flagtypes {
    my $dbh = Bugzilla->dbh;
    # Get all names and IDs, to find broken ones and to
    # check for collisions when renaming.
    my $sth = $dbh->prepare("SELECT name, id FROM flagtypes");
    $sth->execute();

    my %flagtypes;
    my @badflagnames; 
    # find broken flagtype names, and populate a hash table
    # to check for collisions.
    while (my ($name, $id) = $sth->fetchrow_array()) {
        $flagtypes{$name} = $id;
        if ($name =~ /[ ,]/) {
            push(@badflagnames, $name);
        }
    }
    if (@badflagnames) {
        print "Removing spaces and commas from flag names...\n";
        my ($flagname, $tryflagname);
        my $sth = $dbh->prepare("UPDATE flagtypes SET name = ? WHERE id = ?");
        foreach $flagname (@badflagnames) {
            print "  Bad flag type name \"$flagname\" ...\n";
            # find a new name for this flagtype.
            ($tryflagname = $flagname) =~ tr/ ,/__/;
            # avoid collisions with existing flagtype names.
            while (defined($flagtypes{$tryflagname})) {
                print "  ... can't rename as \"$tryflagname\" ...\n";
                $tryflagname .= "'";
                if (length($tryflagname) > 50) {
                    my $lastchanceflagname = (substr $tryflagname, 0, 47) . '...';
                    if (defined($flagtypes{$lastchanceflagname})) {
                        print "  ... last attempt as \"$lastchanceflagname\" still failed.'\n";
                        die install_string('update_flags_bad_name',
                                           { flag => $flagname }), "\n";
                    }
                    $tryflagname = $lastchanceflagname;
                }
            }
            $sth->execute($tryflagname, $flagtypes{$flagname});
            print "  renamed flag type \"$flagname\" as \"$tryflagname\"\n";
            $flagtypes{$tryflagname} = $flagtypes{$flagname};
            delete $flagtypes{$flagname};
        }
        print "... done.\n";
    }
}

sub _setup_usebuggroups_backward_compatibility {
    my $dbh = Bugzilla->dbh;

    # Don't run this on newer Bugzillas. This is a reliable test because
    # the longdescs table existed in 2.16 (which had usebuggroups)
    # but not in 2.18, and this code happens between 2.16 and 2.18.
    return if $dbh->bz_column_info('longdescs', 'already_wrapped');

    # 2002-11-24 - bugreport@peshkin.net - bug 147275
    #
    # If group_control_map is empty, backward-compatibility
    # usebuggroups-equivalent records should be created.
    my ($maps_exist) = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM group_control_map");
    if (!$maps_exist) {
        print "Converting old usebuggroups controls...\n";
        # Initially populate group_control_map.
        # First, get all the existing products and their groups.
        my $sth = $dbh->prepare("SELECT groups.id, products.id, groups.name,
                                        products.name 
                                  FROM groups, products
                                 WHERE isbuggroup != 0");
        $sth->execute();
        while (my ($groupid, $productid, $groupname, $productname)
                = $sth->fetchrow_array()) 
        {
            if ($groupname eq $productname) {
                # Product and group have same name.
                $dbh->do("INSERT INTO group_control_map " .
                         "(group_id, product_id, membercontrol, othercontrol) " .
                         "VALUES (?, ?, ?, ?)", undef,
                         ($groupid, $productid, CONTROLMAPDEFAULT, CONTROLMAPNA));
            } else {
                # See if this group is a product group at all.
                my $sth2 = $dbh->prepare("SELECT id FROM products 
                    WHERE name = " .$dbh->quote($groupname));
                $sth2->execute();
                my ($id) = $sth2->fetchrow_array();
                if (!$id) {
                    # If there is no product with the same name as this
                    # group, then it is permitted for all products.
                    $dbh->do("INSERT INTO group_control_map " .
                             "(group_id, product_id, membercontrol, othercontrol) " .
                             "VALUES (?, ?, ?, ?)", undef,
                             ($groupid, $productid, CONTROLMAPSHOWN, CONTROLMAPNA));
                }
            }
        }
    }
}

sub _remove_user_series_map {
    my $dbh = Bugzilla->dbh;
    # 2004-07-17 GRM - Remove "subscriptions" concept from charting, and add
    # group-based security instead.
    if ($dbh->bz_table_info("user_series_map")) {
        # Oracle doesn't like "date" as a column name, and apparently some DBs
        # don't like 'value' either. We use the changes to subscriptions as
        # something to hang these renamings off.
        $dbh->bz_rename_column('series_data', 'date', 'series_date');
        $dbh->bz_rename_column('series_data', 'value', 'series_value');

        # series_categories.category_id produces a too-long column name for the
        # auto-incrementing sequence (Oracle again).
        $dbh->bz_rename_column('series_categories', 'category_id', 'id');

        $dbh->bz_add_column("series", "public",
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

        # Migrate public-ness across from user_series_map to new field
        my $sth = $dbh->prepare("SELECT series_id from user_series_map " .
                                 "WHERE user_id = 0");
        $sth->execute();
        while (my ($public_series_id) = $sth->fetchrow_array()) {
            $dbh->do("UPDATE series SET public = 1 " .
                     "WHERE series_id = $public_series_id");
        }

        $dbh->bz_drop_table("user_series_map");
    }
}

sub _copy_old_charts_into_database {
    my $dbh = Bugzilla->dbh;
    my $datadir = bz_locations()->{'datadir'};
    # 2003-06-26 Copy the old charting data into the database, and create the
    # queries that will keep it all running. When the old charting system goes
    # away, if this code ever runs, it'll just find no files and do nothing.
    my $series_exists = $dbh->selectrow_array("SELECT 1 FROM series " .
                                              $dbh->sql_limit(1));
    if (!$series_exists && -d "$datadir/mining" && -e "$datadir/mining/-All-") {
        print "Migrating old chart data into database...\n";

        # We prepare the handle to insert the series data
        my $seriesdatasth = $dbh->prepare(
            "INSERT INTO series_data (series_id, series_date, series_value)
                  VALUES (?, ?, ?)");

        my $deletesth = $dbh->prepare(
            "DELETE FROM series_data WHERE series_id = ? AND series_date = ?");

        my $groupmapsth = $dbh->prepare(
            "INSERT INTO category_group_map (category_id, group_id)
                  VALUES (?, ?)");

        # Fields in the data file (matches the current collectstats.pl)
        my @statuses =
            qw(NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED VERIFIED CLOSED);
        my @resolutions =
            qw(FIXED INVALID WONTFIX LATER REMIND DUPLICATE WORKSFORME MOVED);
        my @fields = (@statuses, @resolutions);

        # We have a localization problem here. Where do we get these values?
        my $all_name = "-All-";
        my $open_name = "All Open";

        $dbh->bz_start_transaction();
        my $products = $dbh->selectall_arrayref("SELECT name FROM products");

        foreach my $product ((map { $_->[0] } @$products), "-All-") {
            print "$product:\n";
            # First, create the series
            my %queries;
            my %seriesids;

            my $query_prod = "";
            if ($product ne "-All-") {
                $query_prod = "product=" . html_quote($product) . "&";
            }

            # The query for statuses is different to that for resolutions.
            $queries{$_} = ($query_prod . "bug_status=$_") foreach (@statuses);
            $queries{$_} = ($query_prod . "resolution=$_") 
                foreach (@resolutions);

            foreach my $field (@fields) {
                # Create a Series for each field in this product.
                my $series = new Bugzilla::Series(undef, $product, $all_name,
                                                  $field, undef, 1,
                                                  $queries{$field}, 1);
                $series->writeToDatabase();
                $seriesids{$field} = $series->{'series_id'};
            }

            # We also add a new query for "Open", so that migrated products get
            # the same set as new products (see editproducts.cgi.)
            my @openedstatuses = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED");
            my $query = join("&", map { "bug_status=$_" } @openedstatuses);
            my $series = new Bugzilla::Series(undef, $product, $all_name,
                                              $open_name, undef, 1,
                                              $query_prod . $query, 1);
            $series->writeToDatabase();
            $seriesids{$open_name} = $series->{'series_id'};

            # Now, we attempt to read in historical data, if any
            # Convert the name in the same way that collectstats.pl does
            my $product_file = $product;
            $product_file =~ s/\//-/gs;
            $product_file = "$datadir/mining/$product_file";

            # There are many reasons that this might fail (e.g. no stats 
            # for this product), so we don't worry if it does.
            my $in = new IO::File($product_file) or next;

            # The data files should be in a standard format, even for old
            # Bugzillas, because of the conversion code further up this file.
            my %data;
            my $last_date = "";

            my @lines = <$in>;
            while (my $line = shift @lines) {
                if ($line =~ /^(\d+\|.*)/) {
                    my @numbers = split(/\||\r/, $1);

                    # Only take the first line for each date; it was possible to
                    # run collectstats.pl more than once in a day.
                    next if $numbers[0] eq $last_date;

                    for my $i (0 .. $#fields) {
                        # $numbers[0] is the date
                        $data{$fields[$i]}{$numbers[0]} = $numbers[$i + 1];

                        # Keep a total of the number of open bugs for this day
                        if (grep { $_ eq $fields[$i] } @openedstatuses) {
                            $data{$open_name}{$numbers[0]} += $numbers[$i + 1];
                        }
                    }

                    $last_date = $numbers[0];
                }
            }

            $in->close;

            my $total_items = (scalar(@fields) + 1) 
                              * scalar(keys %{ $data{'NEW'} });
            my $count = 0;
            foreach my $field (@fields, $open_name) {
                # Insert values into series_data: series_id, date, value
                my %fielddata = %{$data{$field}};
                foreach my $date (keys %fielddata) {
                    # We need to delete in case the text file had duplicate 
                    # entries in it.
                    $deletesth->execute($seriesids{$field}, $date);

                    # We prepared this above
                    $seriesdatasth->execute($seriesids{$field},
                                            $date, $fielddata{$date} || 0);
                    indicate_progress({ total => $total_items, 
                                        current => ++$count, every => 100 });
                }
            }

            # Create the groupsets for the category
            my $category_id =
                $dbh->selectrow_array("SELECT id FROM series_categories " .
                                   "WHERE name = " . $dbh->quote($product));
            my $product_id =
                $dbh->selectrow_array("SELECT id FROM products " .
                                   "WHERE name = " . $dbh->quote($product));

            if (defined($category_id) && defined($product_id)) {

                # Get all the mandatory groups for this product
                my $group_ids =
                    $dbh->selectcol_arrayref("SELECT group_id " .
                         "FROM group_control_map " .
                         "WHERE product_id = $product_id " .
                         "AND (membercontrol = " . CONTROLMAPMANDATORY .
                           " OR othercontrol = " . CONTROLMAPMANDATORY . ")");

                foreach my $group_id (@$group_ids) {
                    $groupmapsth->execute($category_id, $group_id);
                }
            }
        }

        $dbh->bz_commit_transaction();
    }
}

sub _add_user_group_map_grant_type {
    my $dbh = Bugzilla->dbh;
    # 2004-04-12 - Keep regexp-based group permissions up-to-date - Bug 240325
    if ($dbh->bz_column_info("user_group_map", "isderived")) {
        $dbh->bz_add_column('user_group_map', 'grant_type',
            {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
        $dbh->do("DELETE FROM user_group_map WHERE isderived != 0");
        $dbh->do("UPDATE user_group_map SET grant_type = " . GRANT_DIRECT);
        $dbh->bz_drop_column("user_group_map", "isderived");

        $dbh->bz_drop_index('user_group_map', 'user_group_map_user_id_idx');
        $dbh->bz_add_index('user_group_map', 'user_group_map_user_id_idx',
            {TYPE => 'UNIQUE',
             FIELDS => [qw(user_id group_id grant_type isbless)]});
    }
}

sub _add_group_group_map_grant_type {
    my $dbh = Bugzilla->dbh;
    # 2004-07-16 - Make it possible to have group-group relationships other than
    # membership and bless.
    if ($dbh->bz_column_info("group_group_map", "isbless")) {
        $dbh->bz_add_column('group_group_map', 'grant_type',
            {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
        $dbh->do("UPDATE group_group_map SET grant_type = " .
                 "IF(isbless, " . GROUP_BLESS . ", " .
                  GROUP_MEMBERSHIP . ")");
        $dbh->bz_drop_index('group_group_map', 'group_group_map_member_id_idx');
        $dbh->bz_drop_column("group_group_map", "isbless");
        $dbh->bz_add_index('group_group_map', 'group_group_map_member_id_idx',
            {TYPE => 'UNIQUE', 
             FIELDS => [qw(member_id grantor_id grant_type)]});
    }
}

sub _add_longdescs_already_wrapped {
    my $dbh = Bugzilla->dbh;
    # 2005-01-29 - mkanat@bugzilla.org
    if (!$dbh->bz_column_info('longdescs', 'already_wrapped')) {
        # Old, pre-wrapped comments should not be auto-wrapped
        $dbh->bz_add_column('longdescs', 'already_wrapped',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'}, 1);
        # If an old comment doesn't have a newline in the first 81 characters,
        # (or doesn't contain a newline at all) and it contains a space,
        # then it's probably a mis-wrapped comment and we should wrap it
        # at display-time.
        print "Fixing old, mis-wrapped comments...\n";
        $dbh->do(q{UPDATE longdescs SET already_wrapped = 0
                    WHERE (} . $dbh->sql_position(q{'\n'}, 'thetext') . q{ > 81
                       OR } . $dbh->sql_position(q{'\n'}, 'thetext') . q{ = 0)
                      AND SUBSTRING(thetext FROM 1 FOR 80) LIKE '% %'});
    }
}

sub _convert_attachments_filename_from_mediumtext {
    my $dbh = Bugzilla->dbh;
    # 2002 November, myk@mozilla.org, bug 178841:
    #
    # Convert the "attachments.filename" column from a ridiculously large
    # "mediumtext" to a much more sensible "varchar(100)".  Also takes
    # the opportunity to remove paths from existing filenames, since they
    # shouldn't be there for security.  Buggy browsers include them,
    # and attachment.cgi now takes them out, but old ones need converting.
    my $ref = $dbh->bz_column_info("attachments", "filename");
    if ($ref->{TYPE} ne 'varchar(100)' && $ref->{TYPE} ne 'varchar(255)') {
        print "Removing paths from filenames in attachments table...";

        my $sth = $dbh->prepare("SELECT attach_id, filename FROM attachments " .
            "WHERE " . $dbh->sql_position(q{'/'}, 'filename') . " > 0 OR " .
            $dbh->sql_position(q{'\\\\'}, 'filename') . " > 0");
        $sth->execute;

        while (my ($attach_id, $filename) = $sth->fetchrow_array) {
            $filename =~ s/^.*[\/\\]//;
            my $quoted_filename = $dbh->quote($filename);
            $dbh->do("UPDATE attachments SET filename = $quoted_filename " .
                     "WHERE attach_id = $attach_id");
        }

        print "Done.\n";

        $dbh->bz_alter_column("attachments", "filename",
                              {TYPE => 'varchar(100)', NOTNULL => 1});
    }
}

sub _rename_votes_count_and_force_group_refresh {
    my $dbh = Bugzilla->dbh;
    # 2003-04-27 - bugzilla@chimpychompy.org (GavinS)
    #
    # Bug 180086 (http://bugzilla.mozilla.org/show_bug.cgi?id=180086)
    #
    # Renaming the 'count' column in the votes table because Sybase doesn't
    # like it
    return if !$dbh->bz_table_info('votes');
    return if $dbh->bz_column_info('votes', 'count');
    $dbh->bz_rename_column('votes', 'count', 'vote_count');
}

sub _fix_group_with_empty_name {
    my $dbh = Bugzilla->dbh;
    # 2005-01-12 Nick Barnes <nb@ravenbrook.com> bug 278010
    # Rename any group which has an empty name.
    # Note that there can be at most one such group (because of
    # the SQL index on the name column).
    my ($emptygroupid) = $dbh->selectrow_array(
        "SELECT id FROM groups where name = ''");
    if ($emptygroupid) {
        # There is a group with an empty name; find a name to rename it
        # as.  Must avoid collisions with existing names.  Start with
        # group_$gid and add _<n> if necessary.
        my $trycount = 0;
        my $trygroupname;
        my $sth = $dbh->prepare("SELECT 1 FROM groups where name = ?");
        my $name_exists = 1;

        while ($name_exists) {
            $trygroupname = "group_$emptygroupid";
            if ($trycount > 0) {
               $trygroupname .= "_$trycount";
            }
            $name_exists = $dbh->selectrow_array($sth, undef, $trygroupname);
            $trycount++;
        }
        $dbh->do("UPDATE groups SET name = ? WHERE id = ?",
                 undef, $trygroupname, $emptygroupid);
        print "Group $emptygroupid had an empty name; renamed as",
              " '$trygroupname'.\n";
    }
}

# A helper for the emailprefs subs below
sub _clone_email_event {
    my ($source, $target) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->do("INSERT INTO email_setting (user_id, relationship, event)
                   SELECT user_id, relationship, $target FROM email_setting
                    WHERE event = $source");
}

sub _migrate_email_prefs_to_new_table {
    my $dbh = Bugzilla->dbh;
    # 2005-03-29 - gerv@gerv.net - bug 73665.
    # Migrate email preferences to new email prefs table.
    if ($dbh->bz_column_info("profiles", "emailflags")) {
        print "Migrating email preferences to new table...\n";

        # These are the "roles" and "reasons" from the original code, mapped to
        # the new terminology of relationships and events.
        my %relationships = ("Owner"     => REL_ASSIGNEE,
                             "Reporter"  => REL_REPORTER,
                             "QAcontact" => REL_QA,
                             "CClist"    => REL_CC,
                             # REL_VOTER was "4" before it was moved to an
                             #  extension.
                             "Voter"     => 4);

        my %events = ("Removeme"    => EVT_ADDED_REMOVED,
                      "Comments"    => EVT_COMMENT,
                      "Attachments" => EVT_ATTACHMENT,
                      "Status"      => EVT_PROJ_MANAGEMENT,
                      "Resolved"    => EVT_OPENED_CLOSED,
                      "Keywords"    => EVT_KEYWORD,
                      "CC"          => EVT_CC,
                      "Other"       => EVT_OTHER,
                      "Unconfirmed" => EVT_UNCONFIRMED);

        # Request preferences
        my %requestprefs = ("FlagRequestee" => EVT_FLAG_REQUESTED,
                            "FlagRequester" => EVT_REQUESTED_FLAG);

        # We run the below code in a transaction to speed things up.
        $dbh->bz_start_transaction();

        # Select all emailflags flag strings
        my $total = $dbh->selectrow_array('SELECT COUNT(*) FROM profiles');
        my $sth = $dbh->prepare("SELECT userid, emailflags FROM profiles");
        $sth->execute();
        my $i = 0;

        while (my ($userid, $flagstring) = $sth->fetchrow_array()) {
            $i++;
            indicate_progress({ total => $total, current => $i, every => 10 });
            # If the user has never logged in since emailprefs arrived, and the
            # temporary code to give them a default string never ran, then
            # $flagstring will be null. In this case, they just get all mail.
            $flagstring ||= "";

            # The 255 param is here, because without a third param, split will
            # trim any trailing null fields, which causes Perl to eject lots of
            # warnings. Any suitably large number would do.
            my %emailflags = split(/~/, $flagstring, 255);

            my $sth2 = $dbh->prepare("INSERT into email_setting " .
                                     "(user_id, relationship, event) VALUES (" .
                                     "$userid, ?, ?)");
            foreach my $relationship (keys %relationships) {
                foreach my $event (keys %events) {
                    my $key = "email$relationship$event";
                    if (!exists($emailflags{$key}) 
                        || $emailflags{$key} eq 'on') 
                    {
                        $sth2->execute($relationships{$relationship},
                                       $events{$event});
                    }
                }
            }
            # Note that in the old system, the value of "excludeself" is 
            # assumed to be off if the preference does not exist in the 
            # user's list, unlike other preferences whose value is 
            # assumed to be on if they do not exist.
            #
            # This preference has changed from global to per-relationship.
            if (!exists($emailflags{'ExcludeSelf'})
                || $emailflags{'ExcludeSelf'} ne 'on')
            {
                foreach my $relationship (keys %relationships) {
                    $dbh->do("INSERT into email_setting " .
                             "(user_id, relationship, event) VALUES (" .
                             $userid . ", " .
                             $relationships{$relationship}. ", " .
                             EVT_CHANGED_BY_ME . ")");
                }
            }

            foreach my $key (keys %requestprefs) {
                if (!exists($emailflags{$key}) || $emailflags{$key} eq 'on') {
                  $dbh->do("INSERT into email_setting " .
                           "(user_id, relationship, event) VALUES (" .
                           $userid . ", " . REL_ANY . ", " .
                           $requestprefs{$key} . ")");
                }
            }
        }
        print "\n";

        # EVT_ATTACHMENT_DATA should initially have identical settings to
        # EVT_ATTACHMENT.
        _clone_email_event(EVT_ATTACHMENT, EVT_ATTACHMENT_DATA);

        $dbh->bz_commit_transaction();
        $dbh->bz_drop_column("profiles", "emailflags");
    }
}

sub _initialize_new_email_prefs {
    my $dbh = Bugzilla->dbh;
    # Check for any "new" email settings that wouldn't have been ported over
    # during the block above.  Since these settings would have otherwise
    # fallen under EVT_OTHER, we'll just clone those settings.  That way if
    # folks have already disabled all of that mail, there won't be any change.
    my %events = (
        "Dependency Tree Changes" => EVT_DEPEND_BLOCK,
        "Product/Component Changes" => EVT_COMPONENT,
    );

    foreach my $desc (keys %events) {
        my $event = $events{$desc};
        my $have_events = $dbh->selectrow_array(
            "SELECT 1 FROM email_setting WHERE event = $event "
            . $dbh->sql_limit(1));

        if (!$have_events) {
            # No settings in the table yet, so we assume that this is the
            # first time it's being set.
            print "Initializing \"$desc\" email_setting ...\n";
            _clone_email_event(EVT_OTHER, $event);
        }
    }
}

sub _change_all_mysql_booleans_to_tinyint {
    my $dbh = Bugzilla->dbh;
    # 2005-03-27: Standardize all boolean fields to plain "tinyint"
    if ( $dbh->isa('Bugzilla::DB::Mysql') ) {
        # This is a change to make things consistent with Schema, so we use
        # direct-database access methods.
        my $quip_info_sth = $dbh->column_info(undef, undef, 'quips', '%');
        my $quips_cols    = $quip_info_sth->fetchall_hashref("COLUMN_NAME");
        my $approved_col  = $quips_cols->{'approved'};
        if ( $approved_col->{TYPE_NAME} eq 'TINYINT'
             and $approved_col->{COLUMN_SIZE} == 1 )
        {
            # series.public could have been renamed to series.is_public,
            # and so wouldn't need to be fixed manually.
            if ($dbh->bz_column_info('series', 'public')) {
                $dbh->bz_alter_column_raw('series', 'public',
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '0'});
            }
            $dbh->bz_alter_column_raw('bug_status', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('rep_platform', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('resolution', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('op_sys', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('bug_severity', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('priority', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('quips', 'approved',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        }
   }
}

# A helper for the below function.
sub _de_dup_version {
    my ($product_id, $version) = @_;
    my $dbh = Bugzilla->dbh;
    print "Fixing duplicate version $version in product_id $product_id...\n";
    $dbh->do('DELETE FROM versions WHERE product_id = ? AND value = ?',
             undef, $product_id, $version);
    $dbh->do('INSERT INTO versions (product_id, value) VALUES (?,?)',
             undef, $product_id, $version);
}

sub _add_versions_product_id_index {
    my $dbh = Bugzilla->dbh;
    if (!$dbh->bz_index_info('versions', 'versions_product_id_idx')) {
        my $dup_versions = $dbh->selectall_arrayref(
            'SELECT product_id, value FROM versions
           GROUP BY product_id, value HAVING COUNT(value) > 1', {Slice=>{}});
        foreach my $dup_version (@$dup_versions) {
            _de_dup_version($dup_version->{product_id}, $dup_version->{value});
        }

        $dbh->bz_add_index('versions', 'versions_product_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});
    }
}

sub _fix_whine_queries_title_and_op_sys_value {
    my $dbh = Bugzilla->dbh;
    if (!exists $dbh->bz_column_info('whine_queries', 'title')->{DEFAULT}) {
        # The below change actually has nothing to do with the whine_queries
        # change, it just has to be contained within a schema change so that
        # it doesn't run every time we run checksetup.

        # Old Bugzillas have "other" as an OS choice, new ones have "Other"
        # (capital O).
        print "Setting any 'other' op_sys to 'Other'...\n";
        $dbh->do('UPDATE op_sys SET value = ? WHERE value = ?',
                 undef, "Other", "other");
        $dbh->do('UPDATE bugs SET op_sys = ? WHERE op_sys = ?',
                 undef, "Other", "other");
        if (Bugzilla->params->{'defaultopsys'} eq 'other') {
            # We can't actually fix the param here, because WriteParams() will
            # make $datadir/params.json unwriteable to the webservergroup.
            # It's too much of an ugly hack to copy the permission-fixing code
            # down to here. (It would create more potential future bugs than
            # it would solve problems.)
            print "WARNING: Your 'defaultopsys' param is set to 'other', but"
                . " Bugzilla now\n"
                . "         uses 'Other' (capital O).\n";
        }

        # Add a DEFAULT to whine_queries stuff so that editwhines.cgi
        # works on PostgreSQL.
        $dbh->bz_alter_column('whine_queries', 'title', {TYPE => 'varchar(128)',
                              NOTNULL => 1, DEFAULT => "''"});
    }
}

sub _fix_attachments_submitter_id_idx {
    my $dbh = Bugzilla->dbh;
    # 2005-06-29 bugreport@peshkin.net, bug 299156
    if ($dbh->bz_index_info('attachments', 'attachments_submitter_id_idx')
        && (scalar(@{$dbh->bz_index_info('attachments',
                                         'attachments_submitter_id_idx'
                                        )->{FIELDS}}) < 2)) 
    {
        $dbh->bz_drop_index('attachments', 'attachments_submitter_id_idx');
    }
    $dbh->bz_add_index('attachments', 'attachments_submitter_id_idx',
                       [qw(submitter_id bug_id)]);
}

sub _copy_attachments_thedata_to_attach_data {
    my $dbh = Bugzilla->dbh;
    # 2005-08-25 - bugreport@peshkin.net - Bug 305333
    if ($dbh->bz_column_info("attachments", "thedata")) {
        print "Migrating attachment data to its own table...\n";
        print "(This may take a very long time)\n";
        $dbh->do("INSERT INTO attach_data (id, thedata)
                       SELECT attach_id, thedata FROM attachments");
        $dbh->bz_drop_column("attachments", "thedata");
    }
}

sub _fix_broken_all_closed_series {
    my $dbh = Bugzilla->dbh;

    # 2005-11-26 - wurblzap@gmail.com - Bug 300473
    # Repair broken automatically generated series queries for non-open bugs.
    my $broken_series_indicator =
        'field0-0-0=resolution&type0-0-0=notequals&value0-0-0=---';
    my $broken_nonopen_series =
        $dbh->selectall_arrayref("SELECT series_id, query FROM series
                                 WHERE query LIKE '$broken_series_indicator%'");
    if (@$broken_nonopen_series) {
        print 'Repairing broken series...';
        my $sth_nuke =
            $dbh->prepare('DELETE FROM series_data WHERE series_id = ?');
        # This statement is used to repair a series by replacing the broken
        # query with the correct one.
        my $sth_repair =
            $dbh->prepare('UPDATE series SET query = ? WHERE series_id = ?');
        # The corresponding series for open bugs look like one of these two
        # variations (bug 225687 changed the order of bug states).
        # This depends on the set of bug states representing open bugs not
        # to have changed since series creation.
        my $open_bugs_query_base_old =
            join("&", map { "bug_status=" . url_quote($_) }
                          ('UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED'));
        my $open_bugs_query_base_new =
            join("&", map { "bug_status=" . url_quote($_) }
                          ('NEW', 'REOPENED', 'ASSIGNED', 'UNCONFIRMED'));
        my $sth_openbugs_series =
            $dbh->prepare("SELECT series_id FROM series WHERE query IN (?, ?)");
        # Statement to find the series which has collected the most data.
        my $sth_data_collected =
            $dbh->prepare('SELECT count(*) FROM series_data 
                            WHERE series_id = ?');
        # Statement to select a broken non-open bugs count data entry.
        my $sth_select_broken_nonopen_data =
            $dbh->prepare('SELECT series_date, series_value FROM series_data' .
                          ' WHERE series_id = ?');
        # Statement to select an open bugs count data entry.
        my $sth_select_open_data =
            $dbh->prepare('SELECT series_value FROM series_data' .
                          ' WHERE series_id = ? AND series_date = ?');
        # Statement to fix a broken non-open bugs count data entry.
        my $sth_fix_broken_nonopen_data =
            $dbh->prepare('UPDATE series_data SET series_value = ?' .
                          ' WHERE series_id = ? AND series_date = ?');
        # Statement to delete an unfixable broken non-open bugs count data 
        # entry.
        my $sth_delete_broken_nonopen_data =
            $dbh->prepare('DELETE FROM series_data' .
                          ' WHERE series_id = ? AND series_date = ?');
        foreach (@$broken_nonopen_series) {
            my ($broken_series_id, $nonopen_bugs_query) = @$_;

            # Determine the product-and-component part of the query.
            if ($nonopen_bugs_query =~ /^$broken_series_indicator(.*)$/) {
                my $prodcomp = $1;

                # If there is more than one series for the corresponding 
                # open-bugs series, we pick the one with the most data,
                # which should be the one which was generated on creation.
                # It's a pity we can't do subselects.
                $sth_openbugs_series->execute(
                    $open_bugs_query_base_old . $prodcomp,
                    $open_bugs_query_base_new . $prodcomp);

                my ($found_open_series_id, $datacount) = (undef, -1);
                foreach my $open_ser_id ($sth_openbugs_series->fetchrow_array) {
                    $sth_data_collected->execute($open_ser_id);
                    my ($this_datacount) = $sth_data_collected->fetchrow_array;
                    if ($this_datacount > $datacount) {
                        $datacount = $this_datacount;
                        $found_open_series_id = $open_ser_id;
                    }
                }

                if ($found_open_series_id) {
                    # Move along corrupted series data and correct it. The
                    # corruption consists of it being the number of all bugs
                    # instead of the number of non-open bugs, so we calculate
                    # the correct count by subtracting the number of open bugs.
                    # If there is no corresponding open-bugs count for some
                    # reason (shouldn't happen), we drop the data entry.
                    print " $broken_series_id...";
                    $sth_select_broken_nonopen_data->execute($broken_series_id);
                    while (my $rowref =
                           $sth_select_broken_nonopen_data->fetchrow_arrayref) 
                    {
                        my ($date, $broken_value) = @$rowref;
                        my ($openbugs_value) =
                            $dbh->selectrow_array($sth_select_open_data, undef,
                                                  $found_open_series_id, $date);
                        if (defined($openbugs_value)) {
                            $sth_fix_broken_nonopen_data->execute
                                ($broken_value - $openbugs_value,
                                 $broken_series_id, $date);
                        }
                        else {
                            print <<EOT;

WARNING - During repairs of series $broken_series_id, the irreparable data
entry for date $date was encountered and is being deleted.

Continuing repairs...
EOT
                            $sth_delete_broken_nonopen_data->execute
                                ($broken_series_id, $date);
                        }
                    }

                    # Fix the broken query so that it collects correct data 
                    # in the future.
                    $nonopen_bugs_query =~
                        s/^$broken_series_indicator/field0-0-0=resolution&type0-0-0=regexp&value0-0-0=./;
                    $sth_repair->execute($nonopen_bugs_query, 
                                         $broken_series_id);
                }
                else {
                    print <<EOT;

WARNING - Series $broken_series_id was meant to collect non-open bug 
counts, but it has counted all bugs instead. It cannot be repaired
automatically because no series that collected open bug counts was found.
You'll probably want to delete or repair collected data for 
series $broken_series_id manually

Continuing repairs...
EOT
                } #  if ($found_open_series_id)
            } #  if ($nonopen_bugs_query =~
        } # foreach (@$broken_nonopen_series)
        print " done.\n";
    } # if (@$broken_nonopen_series)
}

# This needs to happen at two times: when we upgrade from 2.16 (thus creating 
# user_group_map), and when we kill derived gruops in the DB.
sub _rederive_regex_groups {
    my $dbh = Bugzilla->dbh;

    my $regex_groups_exist = $dbh->selectrow_array(
        "SELECT 1 FROM groups WHERE userregexp = '' " . $dbh->sql_limit(1));
    return if !$regex_groups_exist;

    my $regex_derivations = $dbh->selectrow_array(
        'SELECT 1 FROM user_group_map WHERE grant_type = ' . GRANT_REGEXP 
        . ' ' . $dbh->sql_limit(1));
    return if $regex_derivations;

    print "Deriving regex group memberships...\n";

    # Re-evaluate all regexps, to keep them up-to-date.
    my $sth = $dbh->prepare(
        "SELECT profiles.userid, profiles.login_name, groups.id, 
                groups.userregexp, user_group_map.group_id
           FROM (profiles CROSS JOIN groups)
                LEFT JOIN user_group_map
                       ON user_group_map.user_id = profiles.userid
                          AND user_group_map.group_id = groups.id
                          AND user_group_map.grant_type = ?
          WHERE userregexp != '' OR user_group_map.group_id IS NOT NULL");

    my $sth_add = $dbh->prepare(
        "INSERT INTO user_group_map (user_id, group_id, isbless, grant_type)
              VALUES (?, ?, 0, " . GRANT_REGEXP . ")");

    my $sth_del = $dbh->prepare(
        "DELETE FROM user_group_map
          WHERE user_id  = ? AND group_id = ? AND isbless = 0 
                AND grant_type = " . GRANT_REGEXP);

    $sth->execute(GRANT_REGEXP);
    while (my ($uid, $login, $gid, $rexp, $present) = 
               $sth->fetchrow_array()) 
    {
        if ($login =~ m/$rexp/i) {
            $sth_add->execute($uid, $gid) unless $present;
        } else {
            $sth_del->execute($uid, $gid) if $present;
        }
    }
}

sub _clean_control_characters_from_short_desc {
    my $dbh = Bugzilla->dbh;

    # Fixup for Bug 101380
    # "Newlines, nulls, leading/trailing spaces are getting into summaries"

    my $controlchar_bugs =
        $dbh->selectall_arrayref("SELECT short_desc, bug_id FROM bugs WHERE " .
            $dbh->sql_regexp('short_desc', "'[[:cntrl:]]'"));
    if (scalar(@$controlchar_bugs)) {
        my $msg = 'Cleaning control characters from bug summaries...';
        my $found = 0;
        foreach (@$controlchar_bugs) {
            my ($short_desc, $bug_id) = @$_;
            my $clean_short_desc = clean_text($short_desc);
            if ($clean_short_desc ne $short_desc) {
                print $msg if !$found;
                $found = 1;
                print " $bug_id...";
                $dbh->do("UPDATE bugs SET short_desc = ? WHERE bug_id = ?",
                          undef, $clean_short_desc, $bug_id);
            }
        }
        print " done.\n" if $found;
    }
}

sub _stop_storing_inactive_flags {
    my $dbh = Bugzilla->dbh;
    # 2006-03-02 LpSolit@gmail.com - Bug 322285
    # Do not store inactive flags in the DB anymore.
    if ($dbh->bz_column_info('flags', 'id')->{'TYPE'} eq 'INT3') {
        # We first have to remove all existing inactive flags.
        if ($dbh->bz_column_info('flags', 'is_active')) {
            $dbh->do('DELETE FROM flags WHERE is_active = 0');
        }

       # Now we convert the id column to the auto_increment format.
        $dbh->bz_alter_column('flags', 'id',
           {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

        # And finally, we remove the is_active column.
        $dbh->bz_drop_column('flags', 'is_active');
    }
}

sub _change_short_desc_from_mediumtext_to_varchar {
    my $dbh = Bugzilla->dbh;
    # short_desc should not be a mediumtext, fix anything longer than 255 chars.
    if($dbh->bz_column_info('bugs', 'short_desc')->{TYPE} eq 'MEDIUMTEXT') {
        # Move extremely long summaries into a comment ("from" the Reporter),
        # and then truncate the summary.
        my $long_summary_bugs = $dbh->selectall_arrayref(
            'SELECT bug_id, short_desc, reporter
               FROM bugs WHERE CHAR_LENGTH(short_desc) > 255');

        if (@$long_summary_bugs) {
            print "\n", install_string('update_summary_truncated');
            my $comment_sth = $dbh->prepare(
                'INSERT INTO longdescs (bug_id, who, thetext, bug_when)
                      VALUES (?, ?, ?, NOW())');
            my $desc_sth = $dbh->prepare('UPDATE bugs SET short_desc = ?
                                           WHERE bug_id = ?');
            my @affected_bugs;
            foreach my $bug (@$long_summary_bugs) {
                my ($bug_id, $summary, $reporter_id) = @$bug;
                my $summary_comment =
                    install_string('update_summary_truncate_comment',
                                   { summary => $summary });
                $comment_sth->execute($bug_id, $reporter_id, $summary_comment);
                my $short_summary = substr($summary, 0, 252) . "...";
                $desc_sth->execute($short_summary, $bug_id);
                push(@affected_bugs, $bug_id);
            }
            print join(', ', @affected_bugs) . "\n\n";
        }

        $dbh->bz_alter_column('bugs', 'short_desc', {TYPE => 'varchar(255)',
                                                     NOTNULL => 1});
    }
}

sub _move_namedqueries_linkinfooter_to_its_own_table {
    my $dbh = Bugzilla->dbh;
    if ($dbh->bz_column_info("namedqueries", "linkinfooter")) {
        # Move link-in-footer information into a table of its own.
        my $sth_read = $dbh->prepare('SELECT id, userid
                                        FROM namedqueries
                                       WHERE linkinfooter = 1');
        my $sth_write = $dbh->prepare('INSERT INTO namedqueries_link_in_footer
                                       (namedquery_id, user_id) VALUES (?, ?)');
        $sth_read->execute();
        while (my ($id, $userid) = $sth_read->fetchrow_array()) {
            $sth_write->execute($id, $userid);
        }
        $dbh->bz_drop_column("namedqueries", "linkinfooter");
    }
}

sub _add_classifications_sortkey {
    my $dbh = Bugzilla->dbh;
    # 2006-07-07 olav@bkor.dhs.org - Bug 277377
    # Add a sortkey to the classifications
    if (!$dbh->bz_column_info('classifications', 'sortkey')) {
        $dbh->bz_add_column('classifications', 'sortkey',
                            {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});

        my $class_ids = $dbh->selectcol_arrayref(
            'SELECT id FROM classifications ORDER BY name');
        my $sth = $dbh->prepare('UPDATE classifications SET sortkey = ? ' .
                                 'WHERE id = ?');
        my $sortkey = 0;
        foreach my $class_id (@$class_ids) {
            $sth->execute($sortkey, $class_id);
            $sortkey += 100;
        }
    }
}

sub _move_data_nomail_into_db {
    my $dbh = Bugzilla->dbh;
    my $datadir = bz_locations()->{'datadir'};
    # 2006-07-14 karl@kornel.name - Bug 100953
    # If a nomail file exists, move its contents into the DB
    $dbh->bz_add_column('profiles', 'disable_mail',
        { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE' });
    if (-e "$datadir/nomail") {
        # We have a data/nomail file, read it in and delete it
        my %nomail;
        print "Found a data/nomail file.  Moving nomail entries into DB...\n";
        my $nomail_file = new IO::File("$datadir/nomail", 'r');
        while (<$nomail_file>) {
            $nomail{trim($_)} = 1;
        }
        $nomail_file->close;

        # Go through each entry read.  If a user exists, set disable_mail.
        my $query = $dbh->prepare('UPDATE profiles
                                      SET disable_mail = 1
                                    WHERE userid = ?');
        foreach my $user_to_check (keys %nomail) {
            my $uid = $dbh->selectrow_array(
                'SELECT userid FROM profiles WHERE login_name = ?',
                undef, $user_to_check);
            next if !$uid;
            print "\tDisabling email for user $user_to_check\n";
            $query->execute($uid);
            delete $nomail{$user_to_check};
        }

        # If there are any nomail entries remaining, move them to nomail.bad
        # and say something to the user.
        if (scalar(keys %nomail)) {
            print "\n", install_string('update_nomail_bad',
                                       { data => $datadir }), "\n";
            my $nomail_bad = new IO::File("$datadir/nomail.bad", '>>');
            foreach my $unknown_user (keys %nomail) {
                print "\t$unknown_user\n";
                print $nomail_bad "$unknown_user\n";
                delete $nomail{$unknown_user};
            }
            $nomail_bad->close;
            print "\n";
        }

        # Now that we don't need it, get rid of the nomail file.
        unlink "$datadir/nomail";
    }
}

sub _update_longdescs_who_index {
    my $dbh = Bugzilla->dbh;
    # When doing a search on who posted a comment, longdescs is joined
    # against the bugs table. So we need an index on both of these,
    # not just on "who".
    my $who_index = $dbh->bz_index_info('longdescs', 'longdescs_who_idx');
    if (!$who_index || scalar @{$who_index->{FIELDS}} == 1) {
        # If the index doesn't exist, this will harmlessly do nothing.
        $dbh->bz_drop_index('longdescs', 'longdescs_who_idx');
        $dbh->bz_add_index('longdescs', 'longdescs_who_idx', [qw(who bug_id)]);
    }
}

sub _fix_uppercase_custom_field_names {
    # Before the final release of 3.0, custom fields could be
    # created with mixed-case names.
    my $dbh = Bugzilla->dbh;
    my $fields = $dbh->selectall_arrayref(
        'SELECT name, type FROM fielddefs WHERE custom = 1');
    foreach my $row (@$fields) {
        my ($name, $type) = @$row;
        if ($name ne lc($name)) {
            $dbh->bz_rename_column('bugs', $name, lc($name));
            $dbh->bz_rename_table($name, lc($name))
                if $type == FIELD_TYPE_SINGLE_SELECT;
            $dbh->do('UPDATE fielddefs SET name = ? WHERE name = ?',
                     undef, lc($name), $name);
        }
    }
}

sub _fix_uppercase_index_names {
    # We forgot to fix indexes in the above code.
    my $dbh = Bugzilla->dbh;
    my $fields = $dbh->selectcol_arrayref(
        'SELECT name FROM fielddefs WHERE type = ? AND custom = 1',
        undef, FIELD_TYPE_SINGLE_SELECT);
    foreach my $field (@$fields) {
        my $indexes = $dbh->bz_table_indexes($field);
        foreach my $name (keys %$indexes) {
            next if $name eq lc($name);
            my $index = $indexes->{$name};
            # Lowercase the name and everything in the definition.
            my $new_name   = lc($name);
            my @new_fields = map {lc($_)} @{$index->{FIELDS}};
            my $new_def = {FIELDS => \@new_fields, TYPE => $index->{TYPE}};
            $new_def = \@new_fields if !$index->{TYPE};
            $dbh->bz_drop_index($field, $name);
            $dbh->bz_add_index($field, $new_name, $new_def);
        }
    }
}

sub _initialize_workflow_for_upgrade {
    my $old_params = shift;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_add_column('bug_status', 'is_open',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    # Till now, bug statuses were not customizable. Nevertheless, local
    # changes are possible and so we will try to respect these changes.
    # This means: get the status of bugs having a resolution different from ''
    # and mark these statuses as 'closed', even if some of these statuses are
    # expected to be open statuses. Bug statuses we have no information about
    # are left as 'open'.
    #
    # We append the default list of closed statuses *unless* we detect at least
    # one closed state in the DB (i.e. with is_open = 0). This would mean that
    # the DB has already been updated at least once and maybe the admin decided
    # that e.g. 'RESOLVED' is now an open state, in which case we don't want to
    # override this attribute. At least one bug status has to be a closed state
    # anyway (due to the 'duplicate_or_move_bug_status' parameter) so it's safe
    # to use this criteria.
    my $num_closed_states = $dbh->selectrow_array('SELECT COUNT(*) FROM bug_status
                                                   WHERE is_open = 0');

    if (!$num_closed_states) {
        my @closed_statuses =
            @{$dbh->selectcol_arrayref('SELECT DISTINCT bug_status FROM bugs
                                         WHERE resolution != ?', undef, '')};
        @closed_statuses =
          map {$dbh->quote($_)} (@closed_statuses, qw(RESOLVED VERIFIED CLOSED));

        print "Marking closed bug statuses as such...\n";
        $dbh->do('UPDATE bug_status SET is_open = 0 WHERE value IN (' .
                  join(', ', @closed_statuses) . ')');
    }

    # We only populate the workflow here if we're upgrading from a version
    # before 4.0 (which is where init_workflow was added). This was the
    # first schema change done for 4.0, so we check this.
    return if $dbh->bz_column_info('bugs_activity', 'comment_id');

    # Populate the status_workflow table. We do nothing if the table already
    # has entries. If all bug status transitions have been deleted, the
    # workflow will be restored to its default schema.
    my $count = $dbh->selectrow_array('SELECT COUNT(*) FROM status_workflow');

    if (!$count) {
        # Make sure the variables below are defined as
        # status_workflow.require_comment cannot be NULL.
        my $create = $old_params->{'commentoncreate'} || 0;
        my $confirm = $old_params->{'commentonconfirm'} || 0;
        my $accept = $old_params->{'commentonaccept'} || 0;
        my $resolve = $old_params->{'commentonresolve'} || 0;
        my $verify = $old_params->{'commentonverify'} || 0;
        my $close = $old_params->{'commentonclose'} || 0;
        my $reopen = $old_params->{'commentonreopen'} || 0;
        # This was till recently the only way to get back to NEW for
        # confirmed bugs, so we use this parameter here.
        my $reassign = $old_params->{'commentonreassign'} || 0;

        # This is the default workflow for upgrading installations.
        my @workflow = ([undef, 'UNCONFIRMED', $create],
                        [undef, 'NEW', $create],
                        [undef, 'ASSIGNED', $create],
                        ['UNCONFIRMED', 'NEW', $confirm],
                        ['UNCONFIRMED', 'ASSIGNED', $accept],
                        ['UNCONFIRMED', 'RESOLVED', $resolve],
                        ['NEW', 'ASSIGNED', $accept],
                        ['NEW', 'RESOLVED', $resolve],
                        ['ASSIGNED', 'NEW', $reassign],
                        ['ASSIGNED', 'RESOLVED', $resolve],
                        ['REOPENED', 'NEW', $reassign],
                        ['REOPENED', 'ASSIGNED', $accept],
                        ['REOPENED', 'RESOLVED', $resolve],
                        ['RESOLVED', 'UNCONFIRMED', $reopen],
                        ['RESOLVED', 'REOPENED', $reopen],
                        ['RESOLVED', 'VERIFIED', $verify],
                        ['RESOLVED', 'CLOSED', $close],
                        ['VERIFIED', 'UNCONFIRMED', $reopen],
                        ['VERIFIED', 'REOPENED', $reopen],
                        ['VERIFIED', 'CLOSED', $close],
                        ['CLOSED', 'UNCONFIRMED', $reopen],
                        ['CLOSED', 'REOPENED', $reopen]);

        print "Now filling the 'status_workflow' table with valid bug status transitions...\n";
        my $sth_select = $dbh->prepare('SELECT id FROM bug_status WHERE value = ?');
        my $sth = $dbh->prepare('INSERT INTO status_workflow (old_status, new_status,
                                             require_comment) VALUES (?, ?, ?)');

        foreach my $transition (@workflow) {
            my ($from, $to);
            # If it's an initial state, there is no "old" value.
            $from = $dbh->selectrow_array($sth_select, undef, $transition->[0])
              if $transition->[0];
            $to = $dbh->selectrow_array($sth_select, undef, $transition->[1]);
            # If one of the bug statuses doesn't exist, the transition is invalid.
            next if (($transition->[0] && !$from) || !$to);

            $sth->execute($from, $to, $transition->[2] ? 1 : 0);
        }
    }

    # Make sure the bug status used by the 'duplicate_or_move_bug_status'
    # parameter has all the required transitions set.
    my $dup_status = Bugzilla->params->{'duplicate_or_move_bug_status'};
    my $status_id = $dbh->selectrow_array(
        'SELECT id FROM bug_status WHERE value = ?', undef, $dup_status);
    # There's a minor chance that this status isn't in the DB.
    $status_id || return;

    my $missing_statuses = $dbh->selectcol_arrayref(
        'SELECT id FROM bug_status
                        LEFT JOIN status_workflow ON old_status = id
                                                     AND new_status = ?
          WHERE old_status IS NULL', undef, $status_id);

    my $sth = $dbh->prepare('INSERT INTO status_workflow
                             (old_status, new_status) VALUES (?, ?)');

    foreach my $old_status_id (@$missing_statuses) {
        next if ($old_status_id == $status_id);
        $sth->execute($old_status_id, $status_id);
    }
}

sub _make_lang_setting_dynamic {
    my $dbh = Bugzilla->dbh;
    my $count = $dbh->selectrow_array(q{SELECT 1 FROM setting
                                         WHERE name = 'lang'
                                           AND subclass IS NULL});
    if ($count) {
        $dbh->do(q{UPDATE setting SET subclass = 'Lang' WHERE name = 'lang'});
        $dbh->do(q{DELETE FROM setting_value WHERE name = 'lang'});
    }
}

sub _fix_attachment_modification_date {
    my $dbh = Bugzilla->dbh;
    if (!$dbh->bz_column_info('attachments', 'modification_time')) {
        # Allow NULL values till the modification time has been set.
        $dbh->bz_add_column('attachments', 'modification_time', {TYPE => 'DATETIME'});

        print "Setting the modification time for attachments...\n";
        $dbh->do('UPDATE attachments SET modification_time = creation_ts');

        # Now force values to be always defined.
        $dbh->bz_alter_column('attachments', 'modification_time',
                              {TYPE => 'DATETIME', NOTNULL => 1});

        # Update the modification time for attachments which have been modified.
        my $attachments =
          $dbh->selectall_arrayref('SELECT attach_id, MAX(bug_when) FROM bugs_activity
                                    WHERE attach_id IS NOT NULL ' .
                                    $dbh->sql_group_by('attach_id'));

        my $sth = $dbh->prepare('UPDATE attachments SET modification_time = ?
                                 WHERE attach_id = ?');
        $sth->execute($_->[1], $_->[0]) foreach (@$attachments);
    }
    # We add this here to be sure to have the index being added, due to the original
    # patch omitting it.
    $dbh->bz_add_index('attachments', 'attachments_modification_time_idx',
                       [qw(modification_time)]);
}

sub _change_text_types {
    my $dbh = Bugzilla->dbh; 
    return if 
        $dbh->bz_column_info('namedqueries', 'query')->{TYPE} eq 'LONGTEXT';
    _check_content_length('attachments', 'mimetype',    255, 'attach_id');
    _check_content_length('fielddefs',   'description', 255, 'id');
    _check_content_length('attachments', 'description', 255, 'attach_id');

    $dbh->bz_alter_column('bugs', 'bug_file_loc',
        { TYPE => 'MEDIUMTEXT'});
    $dbh->bz_alter_column('longdescs', 'thetext',
        { TYPE => 'LONGTEXT', NOTNULL => 1 });
    $dbh->bz_alter_column('attachments', 'description',
        { TYPE => 'TINYTEXT', NOTNULL => 1 });
    $dbh->bz_alter_column('attachments', 'mimetype',
        { TYPE => 'TINYTEXT', NOTNULL => 1 });
    # This also changes NULL to NOT NULL.
    $dbh->bz_alter_column('flagtypes', 'description',
        { TYPE => 'MEDIUMTEXT', NOTNULL => 1 }, '');
    $dbh->bz_alter_column('fielddefs', 'description',
        { TYPE => 'TINYTEXT', NOTNULL => 1 });
    $dbh->bz_alter_column('groups', 'description',
        { TYPE => 'MEDIUMTEXT', NOTNULL => 1 });
    $dbh->bz_alter_column('namedqueries', 'query',
        { TYPE => 'LONGTEXT', NOTNULL => 1 });

} 

sub _check_content_length {
    my ($table_name, $field_name, $max_length, $id_field) = @_;
    my $dbh = Bugzilla->dbh;
    my %contents = @{ $dbh->selectcol_arrayref(
        "SELECT $id_field, $field_name FROM $table_name 
          WHERE CHAR_LENGTH($field_name) > ?", {Columns=>[1,2]}, $max_length) };

    if (scalar keys %contents) {
        my $error = install_string('install_data_too_long',
                                   { column     => $field_name,
                                     id_column  => $id_field,
                                     table      => $table_name,
                                     max_length => $max_length });
        foreach my $id (keys %contents) {
            my $string = $contents{$id};
            # Don't dump the whole string--it could be 16MB.
            if (length($string) > 80) {
                $string = substr($string, 0, 30) . "..." 
                         . substr($string, -30) . "\n";
            }
            $error .= "$id: $string\n";
        }
        die $error;
    }
}

sub _add_foreign_keys_to_multiselects {
    my $dbh = Bugzilla->dbh;

    my $names = $dbh->selectcol_arrayref(
        'SELECT name 
           FROM fielddefs 
          WHERE type = ' . FIELD_TYPE_MULTI_SELECT);

    foreach my $name (@$names) {
        $dbh->bz_add_fk("bug_$name", "bug_id", 
            {TABLE => 'bugs', COLUMN => 'bug_id', DELETE => 'CASCADE'});
                                                
        $dbh->bz_add_fk("bug_$name", "value",
            {TABLE  => $name, COLUMN => 'value', DELETE => 'RESTRICT'});
    }
}

# This subroutine is used in multiple places (for times when we update
# the text of comments), so it takes an argument, $bug_ids, which causes
# it to update bugs_fulltext for those bug_ids instead of populating the
# whole table.
sub _populate_bugs_fulltext {
    my $bug_ids = shift;
    my $dbh = Bugzilla->dbh;
    my $fulltext = $dbh->selectrow_array('SELECT 1 FROM bugs_fulltext '
                                         . $dbh->sql_limit(1));
    # We only populate the table if it's empty or if we've been given a
    # set of bug ids.
    if ($bug_ids or !$fulltext) {
        $bug_ids ||= $dbh->selectcol_arrayref('SELECT bug_id FROM bugs');
        # If there are no bugs in the bugs table, there's nothing to populate.
        return if !@$bug_ids;
        my $num_bugs = scalar @$bug_ids;

        my $command = "INSERT";
        my $where = "";
        if ($fulltext) {
            print "Updating bugs_fulltext for $num_bugs bugs...\n";
            $where = "WHERE " . $dbh->sql_in('bugs.bug_id', $bug_ids);
            # It turns out that doing a REPLACE INTO is up to 10x faster
            # than any other possible method of updating the table, in MySQL,
            # which matters a LOT for large installations.
            if ($dbh->isa('Bugzilla::DB::Mysql')) {
                $command = "REPLACE";
            }
            else {
                $dbh->do("DELETE FROM bugs_fulltext WHERE " 
                         . $dbh->sql_in('bug_id', $bug_ids));
            }
        }
        else {
            print "Populating bugs_fulltext with $num_bugs entries...";
            print " (this can take a long time.)\n";
        }
        my $newline = $dbh->quote("\n");
        $dbh->do(
         qq{$command INTO bugs_fulltext (bug_id, short_desc, comments, 
                                         comments_noprivate)
                   SELECT bugs.bug_id, bugs.short_desc, }
                 . $dbh->sql_group_concat('longdescs.thetext', $newline, 0)
          . ', ' . $dbh->sql_group_concat('nopriv.thetext',    $newline, 0) .
                 qq{ FROM bugs 
                          LEFT JOIN longdescs
                                 ON bugs.bug_id = longdescs.bug_id
                          LEFT JOIN longdescs AS nopriv
                                 ON longdescs.comment_id = nopriv.comment_id
                                    AND nopriv.isprivate = 0 
                     $where }
                 . $dbh->sql_group_by('bugs.bug_id', 'bugs.short_desc'));
    }
}

sub _fix_illegal_flag_modification_dates {
    my $dbh = Bugzilla->dbh;

    my $rows = $dbh->do('UPDATE flags SET modification_date = creation_date
                         WHERE modification_date < creation_date');
    # If no rows are affected, $dbh->do returns 0E0 instead of 0.
    print "$rows flags had an illegal modification date. Fixed!\n" if ($rows =~ /^\d+$/);
}

sub _add_visiblity_value_to_value_tables {
    my $dbh = Bugzilla->dbh;
    my @standard_fields = 
        qw(bug_status resolution priority bug_severity op_sys rep_platform);
    my $custom_fields = $dbh->selectcol_arrayref(
        'SELECT name FROM fielddefs WHERE custom = 1 AND type IN(?,?)',
        undef, FIELD_TYPE_SINGLE_SELECT, FIELD_TYPE_MULTI_SELECT);
    foreach my $field (@standard_fields, @$custom_fields) {
        $dbh->bz_add_column($field, 'visibility_value_id', {TYPE => 'INT2'});
        $dbh->bz_add_index($field, "${field}_visibility_value_id_idx",
                           ['visibility_value_id']);
    }
}

sub _add_extern_id_index {
    my $dbh = Bugzilla->dbh;
    if (!$dbh->bz_index_info('profiles', 'profiles_extern_id_idx')) {
        # Some Bugzillas have a multiple empty strings in extern_id,
        # which need to be converted to NULLs before we add the index.
        $dbh->do("UPDATE profiles SET extern_id = NULL WHERE extern_id = ''");
        $dbh->bz_add_index('profiles', 'profiles_extern_id_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(extern_id)]});
    }
}

sub _convert_disallownew_to_isactive {
    my $dbh = Bugzilla->dbh;
    if ($dbh->bz_column_info('products', 'disallownew')){
        $dbh->bz_add_column('products', 'isactive', 
                            { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
        
        # isactive is the boolean reverse of disallownew.
        $dbh->do('UPDATE products SET isactive = 0 WHERE disallownew = 1');
        $dbh->do('UPDATE products SET isactive = 1 WHERE disallownew = 0');
        
        $dbh->bz_drop_column('products','disallownew');
    }
}

sub _fix_logincookies_ipaddr {
    my $dbh = Bugzilla->dbh;
    return if !$dbh->bz_column_info('logincookies', 'ipaddr')->{NOTNULL};

    $dbh->bz_alter_column('logincookies', 'ipaddr', {TYPE => 'varchar(40)'});
    $dbh->do('UPDATE logincookies SET ipaddr = NULL WHERE ipaddr = ?',
             undef, '0.0.0.0');
}

sub _fix_invalid_custom_field_names {
    my $fields = Bugzilla->fields({ custom => 1 });

    foreach my $field (@$fields) {
        next if $field->name =~ /^[a-zA-Z0-9_]+$/;
        # The field name is illegal and can break the DB. Kill the field!
        $field->set_obsolete(1);
        print install_string('update_cf_invalid_name',
                             { field => $field->name }), "\n";
        eval { $field->remove_from_db(); };
        warn $@ if $@;
    }
}

sub _set_attachment_comment_type {
    my ($type, $string) = @_;
    my $dbh = Bugzilla->dbh;
    # We check if there are any comments of this type already, first, 
    # because this is faster than a full LIKE search on the comments,
    # and currently this will run every time we run checksetup.
    my $test = $dbh->selectrow_array(
        "SELECT 1 FROM longdescs WHERE type = $type " . $dbh->sql_limit(1));
    return [] if $test;
    my %comments = @{ $dbh->selectcol_arrayref(
        "SELECT comment_id, thetext FROM longdescs
          WHERE thetext LIKE '$string%'", 
        {Columns=>[1,2]}) };
    my @comment_ids = keys %comments;
    return [] if !scalar @comment_ids;
    my $what = "update";
    if ($type == CMT_ATTACHMENT_CREATED) {
        $what = "creation";
    }
    print "Setting the type field on attachment $what comments...\n";
    my $sth = $dbh->prepare(
        'UPDATE longdescs SET thetext = ?, type = ?, extra_data = ?
          WHERE comment_id = ?');
    my $count = 0;
    my $total = scalar @comment_ids;
    foreach my $id (@comment_ids) {
        $count++;
        my $text = $comments{$id};
        next if $text !~ /^\Q$string\E(\d+)/;
        my $attachment_id = $1;
        my @lines = split("\n", $text);
        if ($type == CMT_ATTACHMENT_CREATED) {
            # Now we have to remove the text up until we find a line that's
            # just a single newline, because the old "Created an attachment"
            # text included the attachment description underneath it, and in
            # Bugzillas before 2.20, that could be wrapped into multiple lines,
            # in the database.
            while (1) {
                my $line = shift @lines;
                last if (!defined $line or trim($line) eq '');
            }
        }
        else {
            # However, the "From update of attachment" line is always just
            # one line--the first line of the comment.
            shift @lines;
        }
        $text = join("\n", @lines);
        $sth->execute($text, $type, $attachment_id, $id);
        indicate_progress({ total => $total, current => $count, 
                            every => 25 });
    }
    return \@comment_ids;
}

sub _set_attachment_comment_types {
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    my $created_ids = _set_attachment_comment_type(
        CMT_ATTACHMENT_CREATED, 'Created an attachment (id=');
    my $updated_ids = _set_attachment_comment_type(
        CMT_ATTACHMENT_UPDATED, '(From update of attachment ');
    $dbh->bz_commit_transaction();
    return unless (@$created_ids or @$updated_ids);

    my @comment_ids = (@$created_ids, @$updated_ids);

    my $bug_ids = $dbh->selectcol_arrayref(
        'SELECT DISTINCT bug_id FROM longdescs WHERE '
        . $dbh->sql_in('comment_id', \@comment_ids));
    _populate_bugs_fulltext($bug_ids);
}

sub _add_allows_unconfirmed_to_product_table {
    my $dbh = Bugzilla->dbh;
    if (!$dbh->bz_column_info('products', 'allows_unconfirmed')) {
        $dbh->bz_add_column('products', 'allows_unconfirmed',
            { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE' });
        if ($dbh->bz_column_info('products', 'votestoconfirm')) {
            $dbh->do('UPDATE products SET allows_unconfirmed = 1 
                       WHERE votestoconfirm > 0');
        }
    }
}

sub _convert_flagtypes_fks_to_set_null {
    my $dbh = Bugzilla->dbh;
    foreach my $column (qw(request_group_id grant_group_id)) {
        my $fk = $dbh->bz_fk_info('flagtypes', $column);
        if ($fk and !defined $fk->{DELETE}) {
            $fk->{DELETE} = 'SET NULL';
            $dbh->bz_alter_fk('flagtypes', $column, $fk);
        }
    }
}

sub _fix_decimal_types {
    my $dbh = Bugzilla->dbh;
    my $type = {TYPE => 'decimal(7,2)', NOTNULL => 1, DEFAULT => '0'};
    $dbh->bz_alter_column('bugs', 'estimated_time', $type);
    $dbh->bz_alter_column('bugs', 'remaining_time', $type);
    $dbh->bz_alter_column('longdescs', 'work_time', $type);
}

sub _fix_series_creator_fk {
    my $dbh = Bugzilla->dbh;
    my $fk = $dbh->bz_fk_info('series', 'creator');
    if ($fk and $fk->{DELETE} eq 'SET NULL') {
        $fk->{DELETE} = 'CASCADE';
        $dbh->bz_alter_fk('series', 'creator', $fk);
    }
}

sub _remove_attachment_isurl {
    my $dbh = Bugzilla->dbh;

    if ($dbh->bz_column_info('attachments', 'isurl')) {
        # Now all attachments must have a filename.
        $dbh->do('UPDATE attachments SET filename = ? WHERE isurl = 1',
                 undef, 'url.txt');
        $dbh->bz_drop_column('attachments', 'isurl');
        $dbh->do("DELETE FROM fielddefs WHERE name='attachments.isurl'");
    }
}

sub _add_isactive_to_product_fields {
    my $dbh = Bugzilla->dbh;

    # If we add the isactive column all values should start off as active
    if (!$dbh->bz_column_info('components', 'isactive')) {
        $dbh->bz_add_column('components', 'isactive', 
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
    }
    
    if (!$dbh->bz_column_info('versions', 'isactive')) {
        $dbh->bz_add_column('versions', 'isactive', 
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
    }

    if (!$dbh->bz_column_info('milestones', 'isactive')) {
        $dbh->bz_add_column('milestones', 'isactive', 
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
    }
}

sub _migrate_field_visibility_value {
    my $dbh = Bugzilla->dbh;

    if ($dbh->bz_column_info('fielddefs', 'visibility_value_id')) {
        print "Populating new field_visibility table...\n";

        $dbh->bz_start_transaction();

        my %results =
            @{ $dbh->selectcol_arrayref(
                "SELECT id, visibility_value_id FROM fielddefs
                 WHERE visibility_value_id IS NOT NULL",
               { Columns => [1,2] }) };

        my $insert_sth =
            $dbh->prepare("INSERT INTO field_visibility (field_id, value_id)
                           VALUES (?, ?)");

        foreach my $id (keys %results) {
            $insert_sth->execute($id, $results{$id});
        }

        $dbh->bz_commit_transaction();
        $dbh->bz_drop_column('fielddefs', 'visibility_value_id');
    }
}

sub _fix_series_indexes {
    my $dbh = Bugzilla->dbh;
    return if $dbh->bz_index_info('series', 'series_category_idx');

    $dbh->bz_drop_index('series', 'series_creator_idx');

    # Fix duplicated names under the same category/subcategory before
    # adding the more restrictive index.
    my $duplicated_series = $dbh->selectall_arrayref(
         'SELECT s1.series_id, s1.category, s1.subcategory, s1.name
            FROM series AS s1
      INNER JOIN series AS s2
              ON s1.category = s2.category
             AND s1.subcategory = s2.subcategory
             AND s1.name = s2.name
           WHERE s1.series_id != s2.series_id');
    my $sth_series_update = $dbh->prepare('UPDATE series SET name = ? WHERE series_id = ?');
    my $sth_series_query = $dbh->prepare('SELECT 1 FROM series WHERE name = ?
                                          AND category = ? AND subcategory = ?');

    my %renamed_series;
    foreach my $series (@$duplicated_series) {
        my ($series_id, $category, $subcategory, $name) = @$series;
        # Leave the first series alone, then rename duplicated ones.
        if ($renamed_series{"${category}_${subcategory}_${name}"}++) {
            print "Renaming series ${category}/${subcategory}/${name}...\n";
            my $c = 0;
            my $exists = 1;
            while ($exists) {
                $sth_series_query->execute($name . ++$c, $category, $subcategory);
                $exists = $sth_series_query->fetchrow_array;
            }
            $sth_series_update->execute($name . $c, $series_id);
        }
    }

    $dbh->bz_add_index('series', 'series_creator_idx', ['creator']);
    $dbh->bz_add_index('series', 'series_category_idx',
        {FIELDS => [qw(category subcategory name)], TYPE => 'UNIQUE'});
}

sub _migrate_user_tags {
    my $dbh = Bugzilla->dbh;
    return unless $dbh->bz_column_info('namedqueries', 'query_type');

    my $tags = $dbh->selectall_arrayref('SELECT id, userid, name, query
                                           FROM namedqueries
                                          WHERE query_type != 0');

    my $sth_tags = $dbh->prepare(
        'INSERT INTO tag (user_id, name) VALUES (?, ?)');
    my $sth_tag_id = $dbh->prepare(
        'SELECT id FROM tag WHERE user_id = ? AND name = ?');
    my $sth_bug_tag = $dbh->prepare('INSERT INTO bug_tag (bug_id, tag_id)
                                     VALUES (?, ?)');
    my $sth_nq = $dbh->prepare('UPDATE namedqueries SET query = ?
                                WHERE id = ?');

    if (scalar @$tags) {
        print install_string('update_queries_to_tags'), "\n";
    }

    my $total = scalar(@$tags);
    my $current = 0;

    $dbh->bz_start_transaction();
    foreach my $tag (@$tags) {
        my ($query_id, $user_id, $name, $query) = @$tag;
        # Tags are all lowercase.
        my $tag_name = lc($name);

        $sth_tags->execute($user_id, $tag_name);

        my $tag_id = $dbh->selectrow_array($sth_tag_id,
            undef, $user_id, $tag_name);

        indicate_progress({ current => ++$current, total => $total,
                            every => 25 });

        my $uri = URI->new("buglist.cgi?$query", 'http');
        my $bug_id_list = $uri->query_param_delete('bug_id');
        if (!$bug_id_list) {
            warn "No bug_id param for tag $name from user $user_id: $query";
            next;
        }
        my @bug_ids = split(/[\s,]+/, $bug_id_list);
        # Make sure that things like "001" get converted to "1"
        @bug_ids = map { int($_) } @bug_ids;
        # And remove duplicates
        @bug_ids = uniq @bug_ids;
        foreach my $bug_id (@bug_ids) {
            # If "int" above failed this might be undef. We also
            # don't want to accept bug 0.
            next if !$bug_id;
            $sth_bug_tag->execute($bug_id, $tag_id);
        }

        # Existing tags may be used in whines, or shared with
        # other users. So we convert them rather than delete them.
        $uri->query_param('tag', $tag_name);
        $sth_nq->execute($uri->query, $query_id);
    }

    $dbh->bz_commit_transaction();

    $dbh->bz_drop_column('namedqueries', 'query_type');
}

sub _populate_bug_see_also_class {
    my $dbh = Bugzilla->dbh;

    if ($dbh->bz_column_info('bug_see_also', 'class')) {
        # The length was incorrectly set to 64 instead of 255.
        $dbh->bz_alter_column('bug_see_also', 'class',
                {TYPE => 'varchar(255)', NOTNULL => 1, DEFAULT => "''"});
        return;
    }

    $dbh->bz_add_column('bug_see_also', 'class',
        {TYPE => 'varchar(255)', NOTNULL => 1, DEFAULT => "''"}, '');

    my $result = $dbh->selectall_arrayref(
        "SELECT id, value FROM bug_see_also");

    my $update_sth =
        $dbh->prepare("UPDATE bug_see_also SET class = ? WHERE id = ?");
    
    $dbh->bz_start_transaction();
    foreach my $see_also (@$result) {
        my ($id, $value) = @$see_also;
        my $class = Bugzilla::BugUrl->class_for($value);
        $update_sth->execute($class, $id);
    }
    $dbh->bz_commit_transaction();
}

sub _migrate_disabledtext_boolean {
    my $dbh = Bugzilla->dbh;
    if (!$dbh->bz_column_info('profiles', 'is_enabled')) {
        $dbh->bz_add_column("profiles", 'is_enabled',
                            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
        $dbh->do("UPDATE profiles SET is_enabled = 0 
                  WHERE disabledtext != ''");
    }
}

sub _rename_tags_to_tag {
    my $dbh = Bugzilla->dbh;
    if ($dbh->bz_table_info('tags')) {
        # If we get here, it's because the schema created "tag" as an empty
        # table while "tags" still exists. We get rid of the empty
        # tag table so we can do the rename over the top of it.
        $dbh->bz_drop_table('tag');
        $dbh->bz_drop_index('tags', 'tags_user_id_idx');
        $dbh->bz_rename_table('tags','tag');
        $dbh->bz_add_index('tag', 'tag_user_id_idx',
                           {FIELDS => [qw(user_id name)], TYPE => 'UNIQUE'});
    }
    if (my $bug_tag_fk = $dbh->bz_fk_info('bug_tag', 'tag_id')) {
        # bz_rename_table() didn't handle FKs correctly.
        if ($bug_tag_fk->{TABLE} eq 'tags') {
            $bug_tag_fk->{TABLE} = 'tag';
            $dbh->bz_alter_fk('bug_tag', 'tag_id', $bug_tag_fk);
        }
    }
}

sub _on_delete_set_null_for_audit_log_userid {
    my $dbh = Bugzilla->dbh;
    my $fk = $dbh->bz_fk_info('audit_log', 'user_id');
    if ($fk and !defined $fk->{DELETE}) {
        $fk->{DELETE} = 'SET NULL';
        $dbh->bz_alter_fk('audit_log', 'user_id', $fk);
    }
}

sub _fix_notnull_defaults {
    my $dbh = Bugzilla->dbh;

    $dbh->bz_alter_column('bugs', 'bug_file_loc', 
                          {TYPE => 'MEDIUMTEXT', NOTNULL => 1,  
                           DEFAULT => "''"}, '');

    my $custom_fields = Bugzilla::Field->match({ 
        custom => 1, type => [ FIELD_TYPE_FREETEXT, FIELD_TYPE_TEXTAREA ] 
    });

    foreach my $field (@$custom_fields) {
        if ($field->type == FIELD_TYPE_FREETEXT) {
            $dbh->bz_alter_column('bugs', $field->name,
                                  {TYPE => 'varchar(255)', NOTNULL => 1,
                                   DEFAULT => "''"}, '');
        }
        if ($field->type == FIELD_TYPE_TEXTAREA) {
            $dbh->bz_alter_column('bugs', $field->name,
                                  {TYPE => 'MEDIUMTEXT', NOTNULL => 1,
                                   DEFAULT => "''"}, '');
        }
    }
}

sub _fix_longdescs_primary_key {
    my $dbh = Bugzilla->dbh;
    if ($dbh->bz_column_info('longdescs', 'comment_id')->{TYPE} ne 'INTSERIAL') {
        $dbh->bz_drop_related_fks('longdescs', 'comment_id');
        $dbh->bz_alter_column('bugs_activity', 'comment_id', {TYPE => 'INT4'});
        $dbh->bz_alter_column('longdescs', 'comment_id',
                              {TYPE => 'INTSERIAL',  NOTNULL => 1,  PRIMARYKEY => 1});
    }
}

sub _fix_longdescs_indexes {
    my $dbh = Bugzilla->dbh;
    my $bug_id_idx = $dbh->bz_index_info('longdescs', 'longdescs_bug_id_idx');
    if ($bug_id_idx && scalar @{$bug_id_idx->{'FIELDS'}} < 2) {
        $dbh->bz_drop_index('longdescs', 'longdescs_bug_id_idx');
        $dbh->bz_add_index('longdescs', 'longdescs_bug_id_idx', [qw(bug_id work_time)]);
    }
}

sub _fix_dependencies_dupes {
    my $dbh = Bugzilla->dbh;
    my $blocked_idx = $dbh->bz_index_info('dependencies', 'dependencies_blocked_idx');
    if ($blocked_idx && scalar @{$blocked_idx->{'FIELDS'}} < 2) {
        # Remove duplicated entries
        my $dupes = $dbh->selectall_arrayref("
            SELECT blocked, dependson, COUNT(*) AS count
              FROM dependencies " .
            $dbh->sql_group_by('blocked, dependson') . "
            HAVING COUNT(*) > 1",
            { Slice => {} });
        print "Removing duplicated entries from the 'dependencies' table...\n" if @$dupes;
        foreach my $dupe (@$dupes) {
            $dbh->do("DELETE FROM dependencies
                      WHERE blocked = ? AND dependson = ?",
                     undef, $dupe->{blocked}, $dupe->{dependson});
            $dbh->do("INSERT INTO dependencies (blocked, dependson) VALUES (?, ?)",
                     undef, $dupe->{blocked}, $dupe->{dependson});
        }
        $dbh->bz_drop_index('dependencies', 'dependencies_blocked_idx');
        $dbh->bz_add_index('dependencies', 'dependencies_blocked_idx',
                           { FIELDS => [qw(blocked dependson)], TYPE => 'UNIQUE' });
    }   
}

sub _shorten_long_quips {
    my $dbh = Bugzilla->dbh;
    my $quips = $dbh->selectall_arrayref("SELECT quipid, quip FROM quips
                                          WHERE CHAR_LENGTH(quip) > 512");

    if (@$quips) {
        print "Shortening quips longer than 512 characters:";

        my $query = $dbh->prepare("UPDATE quips SET quip = ? WHERE quipid = ?");

        foreach my $quip (@$quips) {
            my ($quipid, $quip_str) = @$quip;
            $quip_str = substr($quip_str, 0, 509) . "...";
            print " $quipid";
            $query->execute($quip_str, $quipid);
        }
        print "\n";
    }
    $dbh->bz_alter_column('quips', 'quip', { TYPE => 'varchar(512)', NOTNULL => 1});
}

sub _add_password_salt_separator {
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();

    my $profiles = $dbh->selectall_arrayref("SELECT userid, cryptpassword FROM profiles WHERE ("
        . $dbh->sql_regexp("cryptpassword", "'^[^,]+{'") . ")");

    if (@$profiles) {
        say "Adding salt separator to password hashes...";

        my $query = $dbh->prepare("UPDATE profiles SET cryptpassword = ? WHERE userid = ?");
        my %algo_sizes;

        foreach my $profile (@$profiles) {
            my ($userid, $hash) = @$profile;
            my ($algorithm) = $hash =~ /{([^}]+)}$/;

            $algo_sizes{$algorithm} ||= length(Digest->new($algorithm)->b64digest);

            # Calculate the salt length by taking the stored hash and
            # subtracting the combined lengths of the hash size, the
            # algorithm name, and 2 for the {} surrounding the name.
            my $not_salt_len = $algo_sizes{$algorithm} + length($algorithm) + 2;
            my $salt_len = length($hash) - $not_salt_len;

            substr($hash, $salt_len, 0, ',');
            $query->execute($hash, $userid);
        }
    }
    $dbh->bz_commit_transaction();
}

sub _fix_flagclusions_indexes {
    my $dbh = Bugzilla->dbh;
    foreach my $table ('flaginclusions', 'flagexclusions') {
        my $index = $table . '_type_id_idx';
        my $idx_info = $dbh->bz_index_info($table, $index);
        if ($idx_info && $idx_info->{'TYPE'} ne 'UNIQUE') {
            # Remove duplicated entries
            my $dupes = $dbh->selectall_arrayref("
                SELECT type_id, product_id, component_id, COUNT(*) AS count
                  FROM $table " .
                $dbh->sql_group_by('type_id, product_id, component_id') . "
                HAVING COUNT(*) > 1",
                { Slice => {} });
            say "Removing duplicated entries from the '$table' table..." if @$dupes;
            foreach my $dupe (@$dupes) {
                $dbh->do("DELETE FROM $table 
                          WHERE type_id = ? AND product_id = ? AND component_id = ?",
                         undef, $dupe->{type_id}, $dupe->{product_id}, $dupe->{component_id});
                $dbh->do("INSERT INTO $table (type_id, product_id, component_id) VALUES (?, ?, ?)",
                         undef, $dupe->{type_id}, $dupe->{product_id}, $dupe->{component_id});
            }
            $dbh->bz_drop_index($table, $index);
            $dbh->bz_add_index($table, $index,
                { FIELDS => [qw(type_id product_id component_id)],
                  TYPE   => 'UNIQUE' });
        }
    }
}

sub _fix_components_primary_key {
    my $dbh = Bugzilla->dbh;
    if ($dbh->bz_column_info('components', 'id')->{TYPE} ne 'MEDIUMSERIAL') {
        $dbh->bz_drop_related_fks('components', 'id');
        $dbh->bz_alter_column("components", "id",
                              {TYPE => 'MEDIUMSERIAL',  NOTNULL => 1,  PRIMARYKEY => 1});
        $dbh->bz_alter_column("flaginclusions", "component_id",
                              {TYPE => 'INT3'});
        $dbh->bz_alter_column("flagexclusions", "component_id",
                              {TYPE => 'INT3'});
        $dbh->bz_alter_column("bugs", "component_id",
                              {TYPE => 'INT3', NOTNULL => 1});
        $dbh->bz_alter_column("component_cc", "component_id",
                              {TYPE => 'INT3', NOTNULL => 1});
    }
}

sub _fix_user_api_keys_indexes {
    my $dbh = Bugzilla->dbh;

    if ($dbh->bz_index_info('user_api_keys', 'user_api_keys_key')) {
        $dbh->bz_drop_index('user_api_keys', 'user_api_keys_key');
        $dbh->bz_add_index('user_api_keys', 'user_api_keys_api_key_idx',
                           { FIELDS => ['api_key'], TYPE => 'UNIQUE' });
    }
    if ($dbh->bz_index_info('user_api_keys', 'user_api_keys_user_id')) {
        $dbh->bz_drop_index('user_api_keys', 'user_api_keys_user_id');
        $dbh->bz_add_index('user_api_keys', 'user_api_keys_user_id_idx', ['user_id']);
    }
}

sub _update_alias {
    my $dbh = Bugzilla->dbh;
    return unless $dbh->bz_column_info('bugs', 'alias');

    # We need to move the aliases from the bugs table to the bugs_aliases table
    $dbh->do(q{
        INSERT INTO bugs_aliases (bug_id, alias)
        SELECT bug_id, alias FROM bugs WHERE alias IS NOT NULL
    });

    $dbh->bz_drop_column('bugs', 'alias');
}

sub _sanitize_audit_log_table {
    my $dbh = Bugzilla->dbh;

    # Replace hashed passwords by a generic comment.
    my $class = 'Bugzilla::User';
    my $field = 'cryptpassword';

    my $hashed_passwd =
      $dbh->selectcol_arrayref('SELECT added FROM audit_log WHERE class = ? AND field = ?
                                AND ' . $dbh->sql_not_ilike('hashed_with_', 'added'),
                                undef, ($class, $field));
    if (@$hashed_passwd) {
        say "Sanitizing hashed passwords stored in the 'audit_log' table...";
        my $sth = $dbh->prepare('UPDATE audit_log SET added = ?
                                 WHERE class = ? AND field = ? AND added = ?');

        foreach my $passwd (@$hashed_passwd) {
            my (undef, $sanitized_passwd) =
              Bugzilla::Object::_sanitize_audit_log($class, $field, [undef, $passwd]);
            $sth->execute($sanitized_passwd, $class, $field, $passwd);
        }
    }
}

1;

__END__

=head1 NAME

Bugzilla::Install::DB - Fix up the database during installation.

=head1 SYNOPSIS

 use Bugzilla::Install::DB qw(indicate_progress);
 Bugzilla::Install::DB::update_table_definitions();

 indicate_progress({ total => $total, current => $count, every => 10 });

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> to modify the 
database during upgrades.

=head1 SUBROUTINES

=over

=item C<update_table_definitions()>

Description: This is the primary code that updates table definitions
             during upgrades. If you modify the schema in some 
             way, you should add code to the end of this function to 
             make sure that your modifications happen over all installations.

Params:      none

Returns:     nothing

=item C<update_fielddefs_definition()>

Description: L<checksetup.pl> depends on the fielddefs table having
             its schema adjusted before the rest of the tables. So
             these schema updates happen in a separate function from
             L</update_table_definitions()>.

Params:      none

Returns:     nothing

=back
