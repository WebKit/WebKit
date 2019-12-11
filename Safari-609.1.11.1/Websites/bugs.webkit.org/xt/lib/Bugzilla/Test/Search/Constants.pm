# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


# These are constants used by Bugzilla::Test::Search.
# See the comment at the top of that package for a general overview
# of how the search test works, and how the constants are used.
# More detailed information on each constant is available in the comments
# in this file.
package Bugzilla::Test::Search::Constants;
use parent qw(Exporter);
use Bugzilla::Constants;
use Bugzilla::Util qw(generate_random_password);

our @EXPORT = qw(
    ATTACHMENT_FIELDS
    BROKEN_NOT
    COLUMN_TRANSLATION
    COMMENT_FIELDS
    CUSTOM_FIELDS
    CUSTOM_SEARCH_TESTS
    FIELD_SIZE
    FIELD_SUBSTR_SIZE
    FLAG_FIELDS
    INJECTION_BROKEN_FIELD
    INJECTION_BROKEN_OPERATOR
    INJECTION_TESTS
    KNOWN_BROKEN
    NUM_BUGS
    NUM_SEARCH_TESTS
    SKIP_FIELDS
    SPECIAL_PARAM_TESTS
    SUBSTR_NO_FIELD_ADD
    SUBSTR_SIZE
    TESTS
    TESTS_PER_RUN
    USER_FIELDS
);

# Bug 1 is designed to be found by all the "equals" tests. It has
# multiple values for several fields where other fields only have
# one value.
#
# Bug 2 and 3 have a dependency relationship with Bug 1,
# but show up in "not equals" tests. We do use bug 2 in multiple-value
# tests.
#
# Bug 4 should never show up in any equals test, and has no relationship
# with any other bug. However, it does have all its fields set.
#
# Bug 5 only has values set for mandatory fields, to expose problems
# that happen with "not equals" tests failing to catch bugs that don't
# have a value set at all.
#
# Bug 6 is a clone of Bug 1, but is in a group that the searcher isn't
# in.
use constant NUM_BUGS => 6;

# How many tests there are for each operator/field combination other
# than the "contains" tests.
use constant NUM_SEARCH_TESTS => 3;
# This is how many tests get run for each field/operator.
use constant TESTS_PER_RUN => NUM_SEARCH_TESTS + NUM_BUGS;

# This is how many random characters we generate for most fields' names.
# (Some fields can't be this long, though, so they have custom lengths
# in Bugzilla::Test::Search).
use constant FIELD_SIZE => 30;

# These are the custom fields that are created if the BZ_MODIFY_DATABASE_TESTS
# environment variable is set.
use constant CUSTOM_FIELDS => {
    FIELD_TYPE_FREETEXT,  'cf_freetext',
    FIELD_TYPE_SINGLE_SELECT, 'cf_single_select',
    FIELD_TYPE_MULTI_SELECT, 'cf_multi_select',
    FIELD_TYPE_TEXTAREA, 'cf_textarea',
    FIELD_TYPE_DATETIME, 'cf_datetime',
    FIELD_TYPE_BUG_ID, 'cf_bugid',
};

# This translates fielddefs names into Search column names.
use constant COLUMN_TRANSLATION => {
    creation_ts => 'opendate',
    delta_ts    => 'changeddate',
    work_time => 'actual_time',
};

# Make comment field names to their Bugzilla::Comment accessor.
use constant COMMENT_FIELDS => {
    longdesc  => 'body',
    commenter => 'author',
    'longdescs.isprivate' => 'is_private',
};

# Same as above, for Bugzilla::Attachment.
use constant ATTACHMENT_FIELDS => {
    mimetype => 'contenttype',
    submitter => 'attacher',
    thedata   => 'data',
};

# Same, for Bugzilla::Flag.
use constant FLAG_FIELDS => {
    'flagtypes.name' => 'name',
    'setters.login_name' => 'setter',
    'requestees.login_name' => 'requestee',
};

# These are fields that we don't test. Test::More will mark these
# "TODO & SKIP", and not run tests for them at all.
#
# We don't support days_elapsed or owner_idle_time yet.
use constant SKIP_FIELDS => qw(
    owner_idle_time
    days_elapsed
);

# All the fields that represent users.
use constant USER_FIELDS => qw(
    assigned_to
    cc
    reporter
    qa_contact
    commenter
    attachments.submitter
    setters.login_name
    requestees.login_name
);

# For the "substr"-type searches, how short of a substring should
# we use? The goal is to be shorter than the full string, but
# long enough to still be globally unique.
use constant SUBSTR_SIZE => 20;
# However, for some fields, we use a different size.
use constant FIELD_SUBSTR_SIZE => {
    alias => 11,
    # Just the month and day.
    deadline => -5,
    creation_ts => -8,
    delta_ts => -8,
    percentage_complete => 1,
    work_time => 3,
    remaining_time => 3,
    target_milestone => 15,
    longdesc => 25,
    # Just the hour and minute.
    FIELD_TYPE_DATETIME, -5,
};

# For most fields, we add the length of the name of the field plus
# the SUBSTR_SIZE specified above to determine how large of a substring
# we're going to use. However, for some fields, it doesn't make sense to
# add in their field name this way.
use constant SUBSTR_NO_FIELD_ADD => FIELD_TYPE_DATETIME, qw(
    target_milestone remaining_time percentage_complete work_time
    attachments.mimetype attachments.submitter attachments.filename
    attachments.description flagtypes.name
);

################
# Known Broken #
################

# See the KNOWN_BROKEN constant for a general description of these
# "_BROKEN" constants.

# Shared between greaterthan and greaterthaneq.
#
# As with other fields, longdescs greaterthan matches if any comment
# matches (which might be OK).
#
# Same for keywords, and cc. Logically, all of these might
# be OK, but it makes the operation not the logical reverse of
# lessthaneq. What we're really saying here by marking these broken
# is that there ought to be some way of searching "all ccs" vs "any cc"
# (and same for the other fields).
use constant GREATERTHAN_BROKEN => (
    cc        => { contains => [1] },
);

# allwords and allwordssubstr have these broken tests in common.
use constant ALLWORDS_BROKEN => (
    # allwordssubstr on cc fields matches against a single cc,
    # instead of matching against all ccs on a bug.
    cc        => { contains => [1] },
    # bug 828344 changed how these searches operate to revert back to the 4.0
    # behavour, so these tests need to be updated (bug 849117).
    'flagtypes.name' => { contains => [1] },
    longdesc         => { contains => [1] },
);

# Fields that don't generally work at all with changed* searches, but
# probably should.
use constant CHANGED_BROKEN => (
    classification => { contains => [1] },
    commenter => { contains => [1] },
    percentage_complete     => { contains => [1] },
    'requestees.login_name' => { contains => [1] },
    'setters.login_name'    => { contains => [1] },
    delta_ts                => { contains => [1] },
);

# These are additional broken tests that changedfrom and changedto
# have in common.
use constant CHANGED_VALUE_BROKEN => (
    bug_group        => { contains => [1] },
    cc               => { contains => [1] },
    estimated_time   => { contains => [1] },
    'flagtypes.name' => { contains => [1] },
    keywords  => { contains => [1] },
    'longdescs.count' => { search => 1 },
    FIELD_TYPE_MULTI_SELECT, { contains => [1] },
);


# Any test listed in KNOWN_BROKEN gets marked TODO by Test::More
# (using some complex code in Bugzilla::Test::Seach::FieldTest).
# This means that if you run the test under "prove -v", these tests will
# still show up as "not ok", but the test suite results won't show them
# as a failure.
#
# This constant contains operators as keys, which point to hashes. The hashes
# have field names as keys. Each field name points to a hash describing
# how that field/operator combination is broken. The "contains"
# array specifies that that particular "contains" test is expected
# to fail. If "search" is set to 1, then we expect the creation of the
# Bugzilla::Search object to fail.
#
# To allow handling custom fields, you can also use the field type as a key
# instead of the field name. Specifying explicit field names always overrides
# specifying a field type.
#
# Sometimes the operators have multiple tests, and one of them works
# while the other fails. In this case, we have a special override for
# "operator-value", which uniquely identifies tests.
use constant KNOWN_BROKEN => {
    greaterthan   => { GREATERTHAN_BROKEN },
    greaterthaneq => { GREATERTHAN_BROKEN },

    'allwordssubstr-<1>' => { ALLWORDS_BROKEN },
    'allwords-<1>' => {
        ALLWORDS_BROKEN,
    },
    'anywords-<1>' => {
        'flagtypes.name' => { contains => [1,2,3,4,5] },
    },
    'anywords-<1> <2>' => {
        'flagtypes.name' => { contains => [3,4,5] },
    },
    'anywordssubstr-<1> <2>' => {
        'flagtypes.name' => { contains => [3,4,5] },
    },

    # setters.login_name and requestees.login name aren't tracked individually
    # in bugs_activity, so can't be searched using this method.
    #
    # percentage_complete isn't tracked in bugs_activity (and it would be
    # really hard to track). However, it adds a 0=0 term instead of using
    # the changed* charts or simply denying them.
    #
    # delta_ts changedbefore/after should probably search for bugs based
    # on their delta_ts.
    #
    # creation_ts changedbefore/after should search for bug creation dates.
    #
    # The commenter field changedbefore/after should search for comment
    # creation dates.
    #
    # classification isn't being tracked properly in bugs_activity, I think.
    #
    # attach_data.thedata should search when attachments were created and
    # who they were created by.
    'changedbefore' => {
        CHANGED_BROKEN,
        'attach_data.thedata' => { contains => [1] },
    },
    'changedafter' => {
        'attach_data.thedata' => { contains => [2,3,4] },
        classification => { contains => [2,3,4] },
        commenter   => { contains => [2,3,4] },
        delta_ts    => { contains => [2,3,4] },
        percentage_complete => { contains => [2,3,4] },
        'requestees.login_name' => { contains => [2,3,4] },
        'setters.login_name'    => { contains => [2,3,4] },
    },
    changedfrom => {
        CHANGED_BROKEN,
        CHANGED_VALUE_BROKEN,
        # All fields should have a way to search for "changing
        # from a blank value" probably.
        blocked   => { contains => [3,4,5], no_criteria => 1 },
        dependson => { contains => [2,4,5], no_criteria => 1 },
        work_time => { contains => [1] },
        FIELD_TYPE_BUG_ID, { contains => [5], no_criteria => 1 },
    },
    # changeto doesn't find remaining_time changes (possibly due to us not
    # tracking that data properly).
    #
    # multi-valued fields are stored as comma-separated strings, so you
    # can't do changedfrom/to on them.
    #
    # Perhaps commenter can either tell you who the last commenter was,
    # or if somebody commented at a given time (combined with other
    # charts).
    #
    # longdesc changedto/from doesn't do anything; maybe it should.
    # Same for attach_data.thedata.
    changedto => {
        CHANGED_BROKEN,
        CHANGED_VALUE_BROKEN,
        'attach_data.thedata' => { contains => [1] },
        longdesc         => { contains => [1] },
        remaining_time   => { contains => [1] },
    },
    changedby => {
        CHANGED_BROKEN,
        # This should probably search the attacher or anybody who changed
        # anything about an attachment at all.
        'attach_data.thedata' => { contains => [1] },
        # This should probably search the reporter.
        creation_ts => { contains => [1] },
    },
    notequals => {
        'flagtypes.name' => { contains => [1, 5] },
        longdesc         => { contains => [1] },
    },
    notregexp => {
        'flagtypes.name' => { contains => [1, 5] },
        longdesc         => { contains => [1] },
    },
    notsubstring => {
        'flagtypes.name' => { contains => [5] },
        longdesc         => { contains => [1] },
    },
    nowords => {
        'flagtypes.name' => { contains => [1, 5] },
    },
    nowordssubstr => {
        'flagtypes.name' => { contains => [5] },
    },
};

###################
# Broken NotTests #
###################

# Common BROKEN_NOT values for the changed* fields.
use constant CHANGED_BROKEN_NOT => (
    "attach_data.thedata"   => { contains => [1] },
    "classification"        => { contains => [1] },
    "commenter"             => { contains => [1] },
    "delta_ts"              => { contains => [1] },
    percentage_complete     => { contains => [1] },
    "requestees.login_name" => { contains => [1] },
    "setters.login_name"    => { contains => [1] },
);

# For changedfrom and changedto.
use constant CHANGED_FROM_TO_BROKEN_NOT => (
    'longdescs.count' => { search => 1 },
    "bug_group" => { contains => [1] },
    "cc" => { contains => [1] },
    "estimated_time" => { contains => [1] },
    "flagtypes.name" => { contains => [1] },
    "keywords" => { contains => [1] },    
    FIELD_TYPE_MULTI_SELECT, { contains => [1] },
);

# These are field/operator combinations that are broken when run under NOT().
use constant BROKEN_NOT => {
    allwords => {
        cc               => { contains => [1] },
        'flagtypes.name' => { contains => [1, 5] },
        longdesc         => { contains => [1] },
    },
    'allwords-<1> <2>' => {
        cc => { },
    },
    allwordssubstr => {
        cc               => { contains => [1] },
        'flagtypes.name' => { contains => [5, 6] },
        longdesc         => { contains => [1] },
    },
    'allwordssubstr-<1>,<2>' => {
        cc               => { },
        longdesc         => { contains => [1] },
    },
    anyexact => {
        'flagtypes.name' => { contains => [1, 2, 5] },
    },
    'anywords-<1>' => {
        'flagtypes.name' => { contains => [1, 2, 3, 4, 5] },
    },
    'anywords-<1> <2>' => {
        'flagtypes.name' => { contains => [3, 4, 5] },
    },
    anywordssubstr => {
        'flagtypes.name' => { contains => [5] },
    },
    'anywordssubstr-<1> <2>' => {
        'flagtypes.name' => { contains => [3,4,5] },
    },
    casesubstring => {
        'flagtypes.name' => { contains => [5] },
    },
    changedafter => {
        "attach_data.thedata"   => { contains => [2, 3, 4] },
        "classification"        => { contains => [2, 3, 4] },
        "commenter"             => { contains => [2, 3, 4] },
        percentage_complete     => { contains => [2, 3, 4] },
        "delta_ts"              => { contains => [2, 3, 4] },
        "requestees.login_name" => { contains => [2, 3, 4] },
        "setters.login_name"    => { contains => [2, 3, 4] },
    },
    changedbefore => {
        CHANGED_BROKEN_NOT,
    },
    changedby => {
        CHANGED_BROKEN_NOT,
        creation_ts => { contains => [1] },
        work_time   => { contains => [1] },
    },
    changedfrom => {
        CHANGED_BROKEN_NOT,
        CHANGED_FROM_TO_BROKEN_NOT,
        'attach_data.thedata' => { },
        blocked         => { contains => [1, 2] },
        dependson       => { contains => [1, 3] },
        work_time       => { contains => [1] },
        FIELD_TYPE_BUG_ID, { contains => [1 .. 4] },
    },
    changedto => {
        CHANGED_BROKEN_NOT,
        CHANGED_FROM_TO_BROKEN_NOT,
        longdesc => { contains => [1] },
        "remaining_time" => { contains => [1] },
    },
    greaterthan => {
        cc               => { contains => [1] },
        'flagtypes.name' => { contains => [5] },
    },
    greaterthaneq => {
        cc               => { contains => [1] },
        'flagtypes.name' => { contains => [2, 5] },
    },
    equals => {
        'flagtypes.name' => { contains => [1, 5] },
    },
    notequals => {
        longdesc         => { contains => [1] },
    },
    notregexp => {
        longdesc         => { contains => [1] },
    },
    notsubstring => {
        longdesc         => { contains => [1] },
    },
    'nowords-<1>' => {
        'flagtypes.name' => { contains => [5] },
    },
    'nowordssubstr-<1>' => {
        'flagtypes.name' => { contains => [5] },
    },
    lessthan => {
        'flagtypes.name' => { contains => [5] },
    },
    lessthaneq => {
        'flagtypes.name' => { contains => [1, 5] },
    },
    regexp => {
        'flagtypes.name' => { contains => [1, 5] },
        longdesc         => { contains => [1] },
    },
    substring => {
        'flagtypes.name' => { contains => [5] },
        longdesc         => { contains => [1] },
    },
};

#############
# Overrides #
#############

# These overrides are used in the TESTS constant, below.

# Regex tests need unique test values for certain fields.
use constant REGEX_OVERRIDE => {
    'attachments.mimetype'  => { value => '^text/x-1-' },
    bug_file_loc => { value => '^http://1-' },
    see_also  => { value => '^http://1-' },
    blocked   => { value => '^<1>$' },
    dependson => { value => '^<1>$' },
    bug_id    => { value => '^<1>$' },
    'attachments.isobsolete' => { value => '^1'},
    'attachments.ispatch'    => { value => '^1'},
    'attachments.isprivate'  => { value => '^1' },
    cclist_accessible        => { value => '^1' },
    reporter_accessible      => { value => '^1' },
    everconfirmed            => { value => '^1' },
    'longdescs.count'        => { value => '^3' },
    'longdescs.isprivate'    => { value => '^1' },
    creation_ts => { value => '^2037-01-01' },
    delta_ts    => { value => '^2037-01-01' },
    deadline    => { value => '^2037-02-01' },
    estimated_time => { value => '^1.0' },
    remaining_time => { value => '^9.0' },
    work_time      => { value => '^1.0' },
    longdesc       => { value => '^1-' },
    percentage_complete => { value => '^10' },
    FIELD_TYPE_BUG_ID, { value => '^<1>$' },
    FIELD_TYPE_DATETIME, { value => '^2037-03-01' }
};

# Common overrides between lessthan and lessthaneq.
use constant LESSTHAN_OVERRIDE => (
    alias             => { contains => [1,5] },
    estimated_time    => { contains => [1,5] },
    qa_contact        => { contains => [1,5] },
    resolution        => { contains => [1,5] },
    status_whiteboard => { contains => [1,5] },
    FIELD_TYPE_TEXTAREA, { contains => [1,5] },
    FIELD_TYPE_FREETEXT, { contains => [1,5] },
);

# The mandatorily-set fields have values higher than <1>,
# so bug 5 shows up.
use constant GREATERTHAN_OVERRIDE => (
    classification => { contains => [2,3,4,5] },
    assigned_to  => { contains => [2,3,4,5] },
    bug_id       => { contains => [2,3,4,5] },
    bug_group    => { contains => [1,2,3,4] },
    bug_severity => { contains => [2,3,4,5] },
    bug_status   => { contains => [2,3,4,5] },
    component    => { contains => [2,3,4,5] },
    commenter    => { contains => [2,3,4,5] },
    # keywords matches if *any* keyword matches
    keywords     => { contains => [1,2,3,4] },
    longdesc     => { contains => [1,2,3,4] },
    op_sys       => { contains => [2,3,4,5] },
    priority     => { contains => [2,3,4,5] },
    product      => { contains => [2,3,4,5] },
    reporter     => { contains => [2,3,4,5] },
    rep_platform => { contains => [2,3,4,5] },
    short_desc   => { contains => [2,3,4,5] },
    version      => { contains => [2,3,4,5] },
    tag          => { contains => [1,2,3,4] },
    target_milestone => { contains => [2,3,4,5] },
    # Bug 2 is the only bug besides 1 that has a Requestee set.
    'requestees.login_name'  => { contains => [2] },
    FIELD_TYPE_SINGLE_SELECT, { contains => [2,3,4,5] },
    # Override SINGLE_SELECT for resolution.
    resolution => { contains => [2,3,4] },
    # MULTI_SELECTs match if *any* value matches
    FIELD_TYPE_MULTI_SELECT, { contains => [1,2,3,4] },
);

# For all positive multi-value types.
use constant MULTI_BOOLEAN_OVERRIDE => (
    'attachments.ispatch'    => { value => '1,1', contains => [1] },
    'attachments.isobsolete' => { value => '1,1', contains => [1] },
    'attachments.isprivate'  => { value => '1,1', contains => [1] },
    cclist_accessible        => { value => '1,1', contains => [1] },
    reporter_accessible      => { value => '1,1', contains => [1] },
    'longdescs.isprivate'    => { value => '1,1', contains => [1] },
    everconfirmed            => { value => '1,1', contains => [1] },
);

# Same as above, for negative multi-value types.
use constant NEGATIVE_MULTI_BOOLEAN_OVERRIDE => (
    'attachments.ispatch'    => { value => '1,1', contains => [2,3,4,5] },
    'attachments.isobsolete' => { value => '1,1', contains => [2,3,4,5] },
    'attachments.isprivate'  => { value => '1,1', contains => [2,3,4,5] },
    cclist_accessible        => { value => '1,1', contains => [2,3,4,5] },
    reporter_accessible      => { value => '1,1', contains => [2,3,4,5] },
    'longdescs.isprivate'    => { value => '1,1', contains => [2,3,4,5] },
    everconfirmed            => { value => '1,1', contains => [2,3,4,5] },
);

# For anyexact and anywordssubstr
use constant ANY_OVERRIDE => (
    'longdescs.count' => { contains => [1,2,3,4] },
    'work_time' => { value => '1.0,2.0' },
    dependson => { value => '<1>,<3>', contains => [1,3] },
    MULTI_BOOLEAN_OVERRIDE,
);

# For all the changed* searches. The ones that have empty contains
# are fields that never change in value, or will never be rationally
# tracked in bugs_activity.
use constant CHANGED_OVERRIDE => (
    'attachments.submitter' => { contains => [] },
    bug_id    => { contains => [] },
    reporter  => { contains => [] },
    tag       => { contains => [] },
);

#########
# Tests #
#########

# The basic format of this is a hashref, where the keys are operators,
# and each operator has an arrayref of tests that it runs. The tests
# are hashrefs, with the following possible keys:
#
# contains: This is a list of bug numbers that the search is expected
#           to contain. (This is bug numbers, like 1,2,3, not the bug
#           ids. For a description of each bug number, see NUM_BUGS.)
#           Any bug not listed in "contains" must *not* show up in the
#           search result.
# value: The value that you're searching for. There are certain special
#        codes that will be replaced with bug values when the tests are
#        run. In these examples below, "#" indicates a bug number:
#
#        <#> - The field value for this bug.
#
#              For any operator that has the string "word" in it, this is
#              *all* the values for the current field from the numbered bug,
#              joined by a space.
#
#              If the operator has the string "substr" in it, then we
#              take a substring of the value (for single-value searches)
#              or we take a substring of each value and join them (for
#              multi-value "word" searches). The length of the substring
#              is determined by the SUBSTR_SIZE constants above.)
#
#              For other operators, this just becomes the first value from
#              the field for the numbered bug.
#
#              So, if we were running the "equals" test and checking the
#              cc field, <1> would become the login name of the first cc on
#              Bug 1. If we did an "anywords" search test, it would become
#              a space-separated string of the login names of all the ccs
#              on Bug 1. If we did an "anywordssubstr" search test, it would
#              become a space-separated string of the first few characters
#              of each CC's login name on Bug 1.
#              
#        <#-id> - The bug id of the numbered bug.
#        <#-reporter> - The login name of the numbered bug's reporter.
#        <#-delta> - The delta_ts of the numbered bug.
#
# escape: If true, we will call quotemeta() on the value immediately
#         before passing it to Search.pm.
#
# transform: A function to call on any field value before inserting
#            it for a <#> replacement. The transformation function
#            gets all of the bug's values for the field as its arguments.
# if_equal: This allows you to override "contains" for the case where
#           the transformed value (from calling the "transform" function)
#           is equal to the original value.
#
# override: This allows you to override "contains" and "values" for
#           certain fields.
use constant TESTS => {
    equals => [
        { contains => [1], value => '<1>' },
    ],
    notequals => [
        { contains => [2,3,4,5], value => '<1>' },
    ],
    substring => [
        { contains => [1], value => '<1>',
          override => {
              percentage_complete => { contains => [1,2,3] },
          }
        },
    ],
    casesubstring => [
        { contains => [1], value => '<1>',
          override => {
              percentage_complete => { contains => [1,2,3] },
          }
        },
        { contains => [], value => '<1>', transform => sub { lc($_[0]) },
          extra_name => 'lc', if_equal => { contains => [1] },
          override => {
              percentage_complete => { contains => [1,2,3] },
          }
        },
    ],
    notsubstring => [
        { contains => [2,3,4,5], value => '<1>',
          override => {
              percentage_complete => { contains => [4,5] },
          },
        }
    ],
    regexp => [
        { contains => [1], value => '<1>', escape => 1,
          override => {
              percentage_complete => { value => '^10' },
          }
        },
        { contains => [1], value => '^1-', override => REGEX_OVERRIDE },
    ],
    notregexp => [
        { contains => [2,3,4,5], value => '<1>', escape => 1,
          override => {
              percentage_complete => { value => '^10' },
          }
        },
        { contains => [2,3,4,5], value => '^1-', override => REGEX_OVERRIDE },
    ],
    lessthan => [
        { contains => [1], value => 2, 
          override => {
              # A lot of these contain bug 5 because an empty value is validly
              # less than the specified value.
              bug_file_loc => { value => 'http://2-', contains => [1,5] },
              see_also     => { value => 'http://2-' },
              'attachments.mimetype' => { value => 'text/x-2-' },
              blocked   => { value => '<4-id>', contains => [1,2] },
              dependson => { value => '<3-id>', contains => [1,3] },
              bug_id    => { value => '<2-id>' },
              'attachments.isprivate'  => { value => 1, contains => [2,3,4] },
              'attachments.isobsolete' => { value => 1, contains => [2,3,4] },
              'attachments.ispatch'    => { value => 1, contains => [2,3,4] },
              cclist_accessible        => { value => 1, contains => [2,3,4,5] },
              reporter_accessible      => { value => 1, contains => [2,3,4,5] },
              'longdescs.count'        => { value => 3, contains => [2,3,4,5] },
              'longdescs.isprivate'    => { value => 1, contains => [1,2,3,4,5] },
              everconfirmed            => { value => 1, contains => [2,3,4,5] },
              creation_ts => { value => '2037-01-02', contains => [1,5] },
              delta_ts    => { value => '2037-01-02', contains => [1,5] },
              deadline    => { value => '2037-02-02', contains => [1,5] },
              remaining_time => { value => 10, contains => [1,5] },
              percentage_complete => { value => 11, contains => [1,5] },
              longdesc => { value => '2-', contains => [1,5] },
              work_time => { value => 1, contains => [5] },
              FIELD_TYPE_BUG_ID, { value => '<2>', contains => [1,5] },
              FIELD_TYPE_DATETIME, { value => '2037-03-02', contains => [1,5] },
              LESSTHAN_OVERRIDE,
          }
        },
    ],
    lessthaneq => [
        { contains => [1], value => '<1>',
          override => {
              'attachments.isobsolete' => { value => 0, contains => [2,3,4] },
              'attachments.ispatch'    => { value => 0, contains => [2,3,4] },
              'attachments.isprivate'  => { value => 0, contains => [2,3,4] },
              cclist_accessible        => { value => 0, contains => [2,3,4,5] },
              reporter_accessible      => { value => 0, contains => [2,3,4,5] },
              'longdescs.count'        => { value => 2, contains => [2,3,4,5] },
              'longdescs.isprivate'    => { value => -1, contains => [] },
              everconfirmed            => { value => 0, contains => [2,3,4,5] },
              bug_file_loc   => { contains => [1,5] },
              blocked        => { contains => [1,2] },
              deadline       => { contains => [1,5] },
              dependson      => { contains => [1,3] },
              creation_ts    => { contains => [1,5] },
              delta_ts       => { contains => [1,5] },
              remaining_time => { contains => [1,5] },
              longdesc       => { contains => [1,5] },
              percentage_complete => { contains => [1,5] },
              work_time => { value => 1, contains => [1,5] },
              FIELD_TYPE_BUG_ID, { contains => [1,5] },
              FIELD_TYPE_DATETIME, { contains => [1,5] },
              LESSTHAN_OVERRIDE,
          },
        },
    ],
    greaterthan => [
        { contains => [2,3,4], value => '<1>',
          override => {
              dependson => { contains => [3] },
              blocked   => { contains => [2] },
              'attachments.ispatch'    => { value => 0, contains => [1] },
              'attachments.isobsolete' => { value => 0, contains => [1] },
              'attachments.isprivate'  => { value => 0, contains => [1] },
              cclist_accessible        => { value => 0, contains => [1] },
              reporter_accessible      => { value => 0, contains => [1] },
              'longdescs.count'        => { value => 2, contains => [1] },
              'longdescs.isprivate'    => { value => 0, contains => [1] },
              everconfirmed            => { value => 0, contains => [1] },
              'flagtypes.name'         => { value => 2, contains => [2,3,4] },
              GREATERTHAN_OVERRIDE,
          },
        },
    ],
    greaterthaneq => [
        { contains => [2,3,4], value => '<2>',
          override => {
              'attachments.ispatch'    => { value => 1, contains => [1] },
              'attachments.isobsolete' => { value => 1, contains => [1] },
              'attachments.isprivate'  => { value => 1, contains => [1] },
              cclist_accessible        => { value => 1, contains => [1] },
              reporter_accessible      => { value => 1, contains => [1] },
              'longdescs.count'        => { value => 3, contains => [1] },
              'longdescs.isprivate'    => { value => 1, contains => [1] },
              everconfirmed            => { value => 1, contains => [1] },
              dependson => { value => '<3>', contains => [1,3] },
              blocked   => { contains => [1,2] },
              GREATERTHAN_OVERRIDE,
          }
        },
    ],
    matches => [
        { contains => [1], value => '<1>' },
    ],
    notmatches => [
        { contains => [2,3,4,5], value => '<1>' },
    ],
    anyexact => [
        { contains => [1,2], value => '<1>, <2>', 
          override => { ANY_OVERRIDE } },
    ],
    anywordssubstr => [
        { contains => [1,2], value => '<1> <2>', 
          override => {
              ANY_OVERRIDE,
              percentage_complete => { contains => [1,2,3] },
          }
        },
    ],
    allwordssubstr => [
        { contains => [1], value => '<1>',
          override => {
              MULTI_BOOLEAN_OVERRIDE,
              # We search just the number "1" for percentage_complete,
              # which matches a lot of bugs.
              percentage_complete => { contains => [1,2,3] },
          },
        },
        { contains => [], value => '<1>,<2>',
          override => {
              dependson => { value => '<1-id> <3-id>', contains => [] },
              # bug 3 has the value "21" here, so matches "2,1"
              percentage_complete => { value => '<2>,<3>', contains => [3] },
              # 1 0 matches bug 1, which has both public and private comments.
             'longdescs.isprivate' => { contains => [1] },              
          }
        },
    ],
    nowordssubstr => [
        { contains => [2,3,4,5], value => '<1>',
          override => {
              # longdescs.isprivate translates to "1 0", so no bugs should
              # show up.
              'longdescs.isprivate' => { contains => [] },
              percentage_complete => { contains => [4,5] },
              work_time => { contains => [2,3,4,5] },
          }
        },
    ],
    anywords => [
        { contains => [1], value => '<1>',
          override => {
              MULTI_BOOLEAN_OVERRIDE,
          }
        },
        { contains => [1,2], value => '<1> <2>',
          override => {
              MULTI_BOOLEAN_OVERRIDE,
              dependson => { value => '<1> <3>', contains => [1,3] },
              'longdescs.count' => { contains => [1,2,3,4] },              
          },
        },
    ],
    allwords => [
        { contains => [1], value => '<1>',
          override => { MULTI_BOOLEAN_OVERRIDE } },
        { contains => [], value => '<1> <2>',
          override => {
            dependson => { contains => [], value => '<2-id> <3-id>' },
            # 1 0 matches bug 1, which has both public and private comments.
            'longdescs.isprivate' => { contains => [1] },
          }
        },
    ],
    nowords => [
        { contains => [2,3,4,5], value => '<1>',
          override => {
              # longdescs.isprivate translates to "1 0", so no bugs should
              # show up.
              'longdescs.isprivate' => { contains => [] },
              work_time => { contains => [2,3,4,5] },
          }
        },
    ],

    changedbefore => [
        { contains => [1], value => '<1-delta>',
          override => {
              CHANGED_OVERRIDE,
              creation_ts => { contains => [1,5] },
              blocked   => { contains => [1,2] },
              dependson => { contains => [1,3] },
              longdesc => { contains => [1,5] },
              'longdescs.count' => { contains => [1,5] },
          }
        },
    ],
    changedafter => [
        { contains => [2,3,4], value => '<2-delta>',
          override => { 
              CHANGED_OVERRIDE,
              creation_ts => { contains => [3,4] },
              # We only change this for one bug, and it doesn't match.
              'longdescs.isprivate' => { contains => [] },
              # Same for everconfirmed.
              'everconfirmed' => { contains => [] },
              # For blocked and dependson, they have the delta_ts of bug1
              # in the bugs_activity table, so they won't ever match.
              blocked   => { contains => [] },
              dependson => { contains => [] },
          }
        },
    ],
    changedfrom => [
        { contains => [1], value => '<1>',
          override => {
              CHANGED_OVERRIDE,
              # The test never changes an already-set dependency field, but
              # we *can* attempt to test searching against an empty value,
              # which should get us some bugs.
              blocked   => { value => '', contains => [1,2] },
              dependson => { value => '', contains => [1,3] },
              FIELD_TYPE_BUG_ID, { value => '', contains => [1,2,3,4] },
              # longdesc changedfrom doesn't make any sense.
              longdesc => { contains => [] },
              # Nor does creation_ts changedfrom.
              creation_ts => { contains => [] },
              'attach_data.thedata' => { contains => [] },
              bug_id => { value => '<1-id>', contains => [] },
          },
        },
    ],
    changedto => [
        { contains => [1], value => '<1>',
          override => {
              CHANGED_OVERRIDE,
              # I can't imagine any use for creation_ts changedto.
              creation_ts => { contains => [] },
          }
        },
    ],
    changedby => [
        { contains => [1], value => '<1-reporter>',
          override => {
              CHANGED_OVERRIDE,
              blocked   => { contains => [1,2] },
              dependson => { contains => [1,3] },
          },
        },
    ],
    # XXX these need tests developed
    isempty => [],
    isnotempty => [],
};

# Fields that do not behave as we expect, for InjectionTest.
# search => 1 means the Bugzilla::Search creation fails.
# sql_error is a regex that specifies a SQL error that's OK for us to throw.
# operator_ok overrides the "brokenness" of certain operators, so that they
# are always OK for that field/operator combination.
use constant INJECTION_BROKEN_FIELD => {
    # Pg can't run injection tests against integer or date fields. See bug 577557.
    'attachments.isobsolete' => { db_skip => ['Pg'] },
    'attachments.ispatch'    => { db_skip => ['Pg'] },
    'attachments.isprivate'  => { db_skip => ['Pg'] },
    blocked                  => { db_skip => ['Pg'] },
    bug_id                   => { db_skip => ['Pg'] },
    cclist_accessible        => { db_skip => ['Pg'] },
    creation_ts              => { db_skip => ['Pg'] },
    days_elapsed             => { db_skip => ['Pg'] },
    dependson                => { db_skip => ['Pg'] },
    deadline                 => { db_skip => ['Pg'] },
    delta_ts                 => { db_skip => ['Pg'] },
    estimated_time           => { db_skip => ['Pg'] },
    everconfirmed            => { db_skip => ['Pg'] },
    'longdescs.isprivate'    => { db_skip => ['Pg'] },
    percentage_complete      => { db_skip => ['Pg'] },
    remaining_time           => { db_skip => ['Pg'] },
    reporter_accessible      => { db_skip => ['Pg'] },
    work_time                => { db_skip => ['Pg'] },
    FIELD_TYPE_BUG_ID,          { db_skip => ['Pg'] },
    FIELD_TYPE_DATETIME,        { db_skip => ['Pg'] },
    owner_idle_time => { search => 1 },
    'longdescs.count' => {
        search => 1,
        db_skip => ['Pg'],
        operator_ok => [qw(allwords allwordssubstr anywordssubstr casesubstring
                           changedbefore changedafter greaterthan greaterthaneq
                           lessthan lessthaneq notregexp notsubstring
                           nowordssubstr regexp substring anywords
                           notequals nowords equals anyexact)],
    },
};

# Operators that do not behave as we expect, for InjectionTest.
# search => 1 means the Bugzilla::Search creation fails, but
# field_ok contains fields that it does actually succeed for.
use constant INJECTION_BROKEN_OPERATOR => {
    changedafter  => { search => 1, field_ok => ['creation_ts'] },
    changedbefore => { search => 1, field_ok => ['creation_ts'] },
    changedby     => { search => 1 },
    isempty       => { search => 1 },
    isnotempty    => { search => 1 },
};

# Tests run by Bugzilla::Test::Search::InjectionTest.
# We have to make sure the values are all one word or they'll be split
# up by the multi-word tests.
use constant INJECTION_TESTS => (
    { value => ';SEMICOLON_TEST' },
    { value => '--COMMENT_TEST'  },
    { value => "'QUOTE_TEST" },
    { value => "';QUOTE_SEMICOLON_TEST" },
    { value => '/*STAR_COMMENT_TEST' }
);

#################
# Special Tests #
#################

use constant SPECIAL_PARAM_TESTS => (
    { field => 'bug_status', operator => 'anyexact', value => '__open__',
      contains => [5] },
    { field => 'bug_status', operator => 'anyexact', value => '__closed__',
      contains => [1,2,3,4] },
    { field => 'bug_status', operator => 'anyexact', value => '__all__',
      contains => [1,2,3,4,5] },
    
    { field => 'resolution', operator => 'anyexact', value => '---',
      contains => [5] },
    
    # email* query parameters.
    { field => 'assigned_to', operator => 'anyexact',
      value => '<1>, <2-reporter>', contains => [1,2],
      extra_params => { emailreporter1 => 1 } },
    { field => 'assigned_to', operator => 'equals',
      value => '<1>', extra_name => 'email2', contains => [],
      extra_params => {
          email2 => generate_random_password(100), emaillongdesc2 => 1,
      },
    },
    
    # standard pronouns
    { field => 'assigned_to', operator => 'equals', value => '%assignee%',
      contains => [1,2,3,4,5] },
    { field => 'reporter', operator => 'equals', value => '%reporter%',
      contains => [1,2,3,4,5] },
    { field => 'qa_contact', operator => 'equals', value => '%qacontact%',
      contains => [1,2,3,4,5] },
    { field => 'cc', operator => 'equals', value => '%user%',
      contains => [1] },
    # group pronouns
    { field => 'reporter', operator => 'equals',
      value => '%group.<1-bug_group>%', contains => [1,2,3,4,5] },
    { field => 'assigned_to', operator => 'equals',
      value => '%group.<1-bug_group>%', contains => [1,2,3,4,5] },
    { field => 'qa_contact', operator => 'equals',
      value => '%group.<1-bug_group>%', contains => [1,2,3,4] },
    { field => 'cc', operator => 'equals',
      value => '%group.<1-bug_group>%', contains => [1,2,3,4] },
    { field => 'commenter', operator => 'equals',
      value => '%group.<1-bug_group>%', contains => [1,2,3,4,5] },
);

use constant CUSTOM_SEARCH_TESTS => (
    { name => 'OP without CP', contains => [1],
      params => [
          { f => 'OP' },
          { f => 'bug_id', o => 'equals', v => '<1>' },
      ]
    },

    { name => 'Empty OP/CP pair before criteria', contains => [1],
      params => [
          { f => 'OP' }, { f => 'CP' },
          { f => 'bug_id', o => 'equals', v => '<1>' },
      ]
    },

    { name => 'Empty OP/CP pair after criteria', contains => [1],
      params => [
          { f => 'bug_id', o => 'equals', v => '<1>' },
          { f => 'OP' }, { f => 'CP' },
      ]
    },

    { name  => 'empty OP/CP mid criteria', contains => [1],
      columns => ['assigned_to'],
      params => [
          { f => 'bug_id', o => 'equals', v => '<1>' },
          { f => 'OP' }, { f => 'CP' },
          { f => 'assigned_to', o => 'substr', v => '@' },
      ]
    },

    { name  => 'bug_id = 1 AND assigned_to contains @', contains => [1],
      columns => ['assigned_to'],
      params => [
          { f => 'bug_id',      o => 'equals', v => '<1>' },
          { f => 'assigned_to', o => 'substr', v => '@' },
      ]
    },

    { name  => 'NOT(bug_id = 1) AND NOT(assigned_to = 2)',
      contains => [3,4,5],
      columns => ['assigned_to'],
      params => [
          { n => 1, f => 'bug_id',      o => 'equals', v => '<1>' },
          { n => 1, f => 'assigned_to', o => 'equals', v => '<2>' },
      ]
    },

    { name  => 'bug_id = 1 OR assigned_to = 2', contains => [1,2],
      columns => ['assigned_to'], top_params => { j_top => 'OR' },
      params => [
          { f => 'bug_id',      o => 'equals', v => '<1>' },
          { f => 'assigned_to', o => 'equals', v => '<2>' },
      ]
    },

    { name => 'NOT(bug_id = 1 AND assigned_to = 1)', contains => [2,3,4,5],
      columns => ['assigned_to'],
      params => [
          { f => 'OP', n => 1 },
            { f => 'bug_id',      o => 'equals', v => '<1>' },
            { f => 'assigned_to', o => 'equals', v => '<1>' },
          { f => 'CP' },
      ]
    },


    { name  => '(bug_id = 1 AND assigned_to contains @) '
               . ' OR (bug_id = 2 AND assigned_to contains @)',
      contains => [1,2], columns => ['assigned_to'],
      top_params => { j_top => 'OR' },
      params => [
          { f => 'OP' },
            { f => 'bug_id',      o => 'equals', v => '<1>' },
            { f => 'assigned_to', o => 'substr', v => '@' },
          { f => 'CP' },
          { f => 'OP' },
            { f => 'bug_id',      o => 'equals', v => '<2>' },
            { f => 'assigned_to', o => 'substr', v => '@' },
          { f => 'CP' },
      ]
    },

    { name  => '(bug_id = 1 OR assigned_to = 2) '
               . ' AND (bug_id = 2 OR assigned_to = 1)',
      contains => [1,2], columns => ['assigned_to'],
      params => [
          { f => 'OP', j => 'OR' },
            { f => 'bug_id',      o => 'equals', v => '<1>' },
            { f => 'assigned_to', o => 'equals', v => '<2>' },
          { f => 'CP' },
          { f => 'OP', j => 'OR' },
            { f => 'bug_id',      o => 'equals', v => '<2>' },
            { f => 'assigned_to', o => 'equals', v => '<1>' },
          { f => 'CP' },
      ]
    },

    { name  => 'bug_id = 3 OR ( (bug_id = 1 OR assigned_to = 2) '
               . ' AND (bug_id = 2 OR assigned_to = 1) )',
      contains => [1,2,3], columns => ['assigned_to'],
      top_params => { j_top => 'OR' },
      params => [
          { f => 'bug_id', o => 'equals', v => '<3>' },
          { f => 'OP' },
            { f => 'OP', j => 'OR' },
              { f => 'bug_id',      o => 'equals', v => '<1>' },
              { f => 'assigned_to', o => 'equals', v => '<2>' },
            { f => 'CP' },
            { f => 'OP', j => 'OR' },
              { f => 'bug_id',      o => 'equals', v => '<2>' },
              { f => 'assigned_to', o => 'equals', v => '<1>' },
            { f => 'CP' },
          { f => 'CP' },
      ]
    },

    { name  => 'bug_id = 3 OR ( (bug_id = 1 OR assigned_to = 2) '
               . ' AND (bug_id = 2 OR assigned_to = 1) ) OR bug_id = 4',
      contains => [1,2,3,4], columns => ['assigned_to'],
      top_params => { j_top => 'OR' },
      params => [
          { f => 'bug_id', o => 'equals', v => '<3>' },
          { f => 'OP' },
            { f => 'OP', j => 'OR' },
              { f => 'bug_id',      o => 'equals', v => '<1>' },
              { f => 'assigned_to', o => 'equals', v => '<2>' },
            { f => 'CP' },
            { f => 'OP', j => 'OR' },
              { f => 'bug_id',      o => 'equals', v => '<2>' },
              { f => 'assigned_to', o => 'equals', v => '<1>' },
            { f => 'CP' },
          { f => 'CP' },
          { f => 'bug_id', o => 'equals', v => '<4>' },
      ]
    },

);

1;
