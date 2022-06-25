# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module tests Bugzilla/Search.pm. It uses various constants
# that are in Bugzilla::Test::Search::Constants, in xt/lib/.
#
# It does this by:
# 1) Creating a bunch of field values. Each field value is
#    randomly named and fully unique.
# 2) Creating a bunch of bugs that use those unique field
#    values. Each bug has different characteristics--see
#    the comment above the NUM_BUGS constant for a description
#    of each bug.
# 3) Running searches using the combination of every search operator against
#    every field. The tests that we run are described by the TESTS constant.
#    Some of the operator/field combinations are known to be broken--
#    these are listed in the KNOWN_BROKEN constant.
# 4) For each search, we make sure that certain bugs are contained in
#    the search, and certain other bugs are not contained in the search.
#    The code for the operator/field tests is mostly in
#    Bugzilla::Test::Search::FieldTest.
# 5) After testing each operator/field combination's functionality, we
#    do additional tests to make sure that there are no SQL injections
#    possible via any operator/field combination. The code for the
#    SQL Injection tests is in Bugzilla::Test::Search::InjectionTest.
#
# Generally, the only way that you should modify the behavior of this
# script is by modifying the constants.

package Bugzilla::Test::Search;

use strict;
use warnings;
use Bugzilla::Attachment;
use Bugzilla::Bug ();
use Bugzilla::Constants;
use Bugzilla::Field;
use Bugzilla::Field::Choice;
use Bugzilla::FlagType;
use Bugzilla::Group;
use Bugzilla::Install ();
use Bugzilla::Test::Search::Constants;
use Bugzilla::Test::Search::CustomTest;
use Bugzilla::Test::Search::FieldTestNormal;
use Bugzilla::Test::Search::OperatorTest;
use Bugzilla::User ();
use Bugzilla::Util qw(generate_random_password);

use Carp;
use DateTime;
use Scalar::Util qw(blessed);

###############
# Constructor #
###############

sub new {
    my ($class, $options) = @_;
    return bless { options => $options }, $class;
}

#############
# Accessors #
#############

sub options { return $_[0]->{options} }
sub option { return $_[0]->{options}->{$_[1]} }

sub num_tests {
    my ($self) = @_;
    my @top_operators = $self->top_level_operators;
    my @all_operators = $self->all_operators;
    my $top_operator_tests = $self->_total_operator_tests(\@top_operators);
    my $all_operator_tests = $self->_total_operator_tests(\@all_operators);

    my @fields = $self->all_fields;

    # Basically, we run TESTS_PER_RUN tests for each field/operator combination.
    my $top_combinations = $top_operator_tests * scalar(@fields);
    my $all_combinations = $all_operator_tests * scalar(@fields);
    # But we also have ORs, for which we run combinations^2 tests.
    my $join_tests = $self->option('long')
                     ? ($top_combinations * $all_combinations) : 0;
    # And AND tests, which means we run 2x $join_tests;
    $join_tests = $join_tests * 2;
    # Also, because of NOT tests and Normal tests, we run 3x $top_combinations.
    my $basic_tests = $top_combinations * 3;
    my $operator_field_tests = ($basic_tests + $join_tests) * TESTS_PER_RUN;

    # Then we test each field/operator combination for SQL injection.
    my @injection_values = INJECTION_TESTS;
    my $sql_injection_tests = scalar(@fields) * scalar(@top_operators)
                              * scalar(@injection_values) * NUM_SEARCH_TESTS;

    # This @{ [] } thing is the only reasonable way to get a count out of a
    # constant array.
    my $special_tests = scalar(@{ [SPECIAL_PARAM_TESTS, CUSTOM_SEARCH_TESTS] }) 
                        * TESTS_PER_RUN;
    
    return $operator_field_tests + $sql_injection_tests + $special_tests;
}

sub _total_operator_tests {
    my ($self, $operators) = @_;
    
    # Some operators have more than one test. Find those ones and add
    # them to the total operator tests
    my $extra_operator_tests;
    foreach my $operator (@$operators) {
        my $tests = TESTS->{$operator};
        next if !$tests;
        my $extra_num = scalar(@$tests) - 1;
        $extra_operator_tests += $extra_num;
    }
    return scalar(@$operators) + $extra_operator_tests;
    
}

sub all_operators {
    my ($self) = @_;
    if (not $self->{all_operators}) {
        
        my @operators;
        if (my $limit_operators = $self->option('operators')) {
            @operators = split(',', $limit_operators);
        }
        else {
            @operators = sort (keys %{ Bugzilla::Search::OPERATORS() });
        }
        # "substr" is just a backwards-compatibility operator, same as "substring".
        @operators = grep { $_ ne 'substr' } @operators;
        $self->{all_operators} = \@operators;
    }
    return @{ $self->{all_operators} };
}

sub all_fields {
    my $self = shift;
    if (not $self->{all_fields}) {
        $self->_create_custom_fields();
        my @fields = @{ Bugzilla->fields };
        @fields = sort { $a->name cmp $b->name } @fields;
        $self->{all_fields} = \@fields;
    }
    return @{ $self->{all_fields} };
}

sub top_level_operators {
    my ($self) = @_;
    if (!$self->{top_level_operators}) {
        my @operators;
        my $limit_top = $self->option('top-operators');
        if ($limit_top) {
            @operators = split(',', $limit_top);
        }
        else {
            @operators = $self->all_operators;
        }
        $self->{top_level_operators} = \@operators;
    }
    return @{ $self->{top_level_operators} };
}

sub text_fields {
    my ($self) = @_;
    my @text_fields = grep { $_->type == FIELD_TYPE_TEXTAREA
                             or $_->type == FIELD_TYPE_FREETEXT } $self->all_fields;
    @text_fields = map { $_->name } @text_fields;
    push(@text_fields, qw(short_desc status_whiteboard bug_file_loc see_also));
    return @text_fields;
}

sub bugs {
    my $self = shift;
    $self->{bugs} ||= [map { $self->_create_one_bug($_) } (1..NUM_BUGS)];
    return @{ $self->{bugs} };
}

# Get a numbered bug.
sub bug {
    my ($self, $number) = @_;
    return ($self->bugs)[$number - 1];
}

sub admin {
    my $self = shift;
    if (!$self->{admin_user}) {
        my $admin = create_user("admin");
        Bugzilla::Install::make_admin($admin);
        $self->{admin_user} = $admin;
    }
    # We send back a fresh object every time, to make sure that group
    # memberships are always up-to-date.
    return new Bugzilla::User($self->{admin_user}->id);
}

sub nobody {
    my $self = shift;
    $self->{nobody} ||= Bugzilla::Group->create({ name => "nobody-" . random(),
        description => "Nobody", isbuggroup => 1 });
    return $self->{nobody};
}
sub everybody {
    my ($self) = @_;
    $self->{everybody} ||= create_group('To The Limit');
    return $self->{everybody};
}

sub bug_create_value {
    my ($self, $number, $field) = @_;
    $field = $field->name if blessed($field);
    if ($number == 6 and $field ne 'alias') {
        $number = 1;
    }
    my $extra_values = $self->_extra_bug_create_values->{$number};
    if (exists $extra_values->{$field}) {
        return $extra_values->{$field};
    }
    return $self->_bug_create_values->{$number}->{$field};
}
sub bug_update_value {
    my ($self, $number, $field) = @_;
    $field = $field->name if blessed($field);
    if ($number == 6 and $field ne 'alias') {
        $number = 1;
    }
    return $self->_bug_update_values->{$number}->{$field};
}

# Values used to create the bugs.
sub _bug_create_values {
    my $self = shift;
    return $self->{bug_create_values} if $self->{bug_create_values};
    my %values;
    foreach my $number (1..NUM_BUGS) {
        $values{$number} = $self->_create_field_values($number, 'for create');
    }
    $self->{bug_create_values} = \%values;
    return $self->{bug_create_values};
}
# Values as they existed on the bug, at creation time. Used by the
# changedfrom tests.
sub _extra_bug_create_values {
    my $self = shift;
    $self->{extra_bug_create_values} ||= { map { $_ => {} } (1..NUM_BUGS) };
    return $self->{extra_bug_create_values};
}

# Values used to update the bugs after they are created.
sub _bug_update_values {
    my $self = shift;
    return $self->{bug_update_values} if $self->{bug_update_values};
    my %values;
    foreach my $number (1..NUM_BUGS) {
        $values{$number} = $self->_create_field_values($number);
    }
    $self->{bug_update_values} = \%values;
    return $self->{bug_update_values};
}

##############################
# General Helper Subroutines #
##############################

sub random {
    $_[0] ||= FIELD_SIZE;
    generate_random_password(@_);
}

# We need to use a custom timestamp for each create() and update(),
# because the database returns the same value for LOCALTIMESTAMP(0)
# for the entire transaction, and we need each created bug to have
# its own creation_ts and delta_ts.
sub timestamp {
    my ($day, $second) = @_;
    return DateTime->new(
        year   => 2037,
        month  => 1,
        day    => $day,
        hour   => 12,
        minute => $second,
        second => 0,
        # We make it floating because the timezone doesn't matter for our uses,
        # and we want totally consistent behavior across all possible machines.
        time_zone => 'floating',
    );
}

sub create_keyword {
    my ($number) = @_;
    return Bugzilla::Keyword->create({
        name => "$number-keyword-" . random(),
        description => "Keyword $number" });
}

sub create_user {
    my ($prefix) = @_;
    my $user_name = $prefix . '-' . random(15) . "@" . random(12)
                    . "." . random(3);
    my $user_realname = $prefix . '-' . random();
    my $user = Bugzilla::User->create({
        login_name => $user_name,
        realname   => $user_realname,
        cryptpassword => '*',
    });
    return $user;
}

sub create_group {
    my ($prefix) = @_;
    return Bugzilla::Group->create({
        name => "$prefix-group-" . random(), description => "Everybody $prefix",
        userregexp => '.*', isbuggroup => 1 });
}

sub create_legal_value {
    my ($field, $number) = @_;
    my $type = Bugzilla::Field::Choice->type($field);
    my $field_name = $field->name;
    return $type->create({ value => "$number-$field_name-" . random(),
                           is_open => 0 });
}

#########################
# Custom Field Creation #
#########################

sub _create_custom_fields {
    my ($self) = @_;
    return if !$self->option('add-custom-fields');
    
    while (my ($type, $name) = each %{ CUSTOM_FIELDS() }) {
        my $exists = new Bugzilla::Field({ name => $name });
        next if $exists;
        Bugzilla::Field->create({
            name => $name,
            type => $type,
            description => "Search Test Field $name",
            enter_bug => 1,
            custom => 1,
            buglist => 1,
            is_mandatory => 0,
        });
    }
}

########################
# Field Value Creation #
########################

sub _create_field_values {
    my ($self, $number, $for_create) = @_;
    my $dbh = Bugzilla->dbh;
    
    Bugzilla->set_user($self->admin);

    my @selects = grep { $_->is_select } $self->all_fields;
    my %values;
    foreach my $field (@selects) {
        next if $field->is_abnormal;
        $values{$field->name} = create_legal_value($field, $number)->name;
    }

    my $group = create_group($number);
    $values{groups} = [$group->name];

    $values{'keywords'} = create_keyword($number)->name;

    foreach my $field (qw(assigned_to qa_contact reporter cc)) {
        $values{$field} = create_user("$number-$field")->login;
    }

    my $classification = Bugzilla::Classification->create(
        { name => "$number-classification-" . random() });
    $classification = $classification->name;

    my $version = "$number-version-" . random();
    my $milestone = "$number-tm-" . random(15);
    my $product = Bugzilla::Product->create({
        name => "$number-product-" . random(),
        description => 'Created by t/search.t',
        defaultmilestone => $milestone,
        classification => $classification,
        version => $version,
        allows_unconfirmed => 1,
    });
    foreach my $item ($group, $self->nobody) {
        $product->set_group_controls($item,
            { membercontrol => CONTROLMAPSHOWN,
              othercontrol => CONTROLMAPNA });
    }
    # $product->update() is called lower down.
    my $component = Bugzilla::Component->create({
        product => $product, name => "$number-component-" . random(),
        initialowner => create_user("$number-defaultowner")->login,
        initialqacontact => create_user("$number-defaultqa")->login,
        initial_cc => [create_user("$number-initcc")->login],
        description => "Component $number" });
    
    $values{'product'} = $product->name;
    $values{'component'} = $component->name;
    $values{'target_milestone'} = $milestone;
    $values{'version'} = $version;

    foreach my $field ($self->text_fields) {
        # We don't add a - after $field for the text fields, because
        # if we do, fulltext searching for short_desc pulls out
        # "short_desc" as a word and matches it in every bug.
        my $value = "$number-$field" . random();
        if ($field eq 'bug_file_loc' or $field eq 'see_also') {
            $value = "http://$value-" . random(3)
                     . "/show_bug.cgi?id=$number";
        }
        $values{$field} = $value;
    }
    $values{'tag'} = ["$number-tag-" . random()];
    
    my @date_fields = grep { $_->type == FIELD_TYPE_DATETIME } $self->all_fields;
    foreach my $field (@date_fields) {
        # We use 03 as the month because that differs from our creation_ts,
        # delta_ts, and deadline. (It's nice to have recognizable values
        # for each field when debugging.)
        my $second = $for_create ? $number : $number + 1;
        $values{$field->name} = "2037-03-0$number 12:34:0$second";
    }

    $values{alias} = "$number-alias-" . random(12);

    # Prefixing the original comment with "description" makes the
    # lesserthan and greaterthan tests behave predictably.
    my $comm_prefix = $for_create ? "description-" : '';
    $values{comment} = "$comm_prefix$number-comment-" . random()
                               . ' ' . random();

    my @flags;
    my $setter = create_user("$number-setters.login_name");
    my $requestee = create_user("$number-requestees.login_name");
    $values{set_flags} = _create_flags($number, $setter, $requestee);

    my $month = $for_create ? "12" : "02";
    $values{'deadline'} = "2037-$month-0$number";
    my $estimate_times = $for_create ? 10 : 1;
    $values{estimated_time} = $estimate_times * $number;

    $values{attachment} = _get_attach_values($number, $for_create);

    # Some things only happen on the first bug.
    if ($number == 1) {
        # We use 6 as the prefix for the extra values, because bug 6's values
        # don't otherwise get used (since bug 6 is created as a clone of
        # bug 1). This also makes sure that our greaterthan/lessthan
        # tests work properly.
        my $extra_group = create_group(6);
        $product->set_group_controls($extra_group,
            { membercontrol => CONTROLMAPSHOWN,
              othercontrol => CONTROLMAPNA });
        $values{groups} = [$values{groups}->[0], $extra_group->name];
        my $extra_keyword = create_keyword(6);
        $values{keywords} = [$values{keywords}, $extra_keyword->name];
        my $extra_cc = create_user("6-cc");
        $values{cc} = [$values{cc}, $extra_cc->login];
        my @multi_selects = grep { $_->type == FIELD_TYPE_MULTI_SELECT }
                                 $self->all_fields;
        foreach my $field (@multi_selects) {
            my $new_value = create_legal_value($field, 6);
            my $name = $field->name;
            $values{$name} = [$values{$name}, $new_value->name];
        }
        push(@{ $values{'tag'} }, "6-tag-" . random());
    }

    # On bug 5, any field that *can* be left empty, *is* left empty.
    if ($number == 5) {
        my @set_fields = grep { $_->type == FIELD_TYPE_SINGLE_SELECT }
                         $self->all_fields;
        @set_fields = map { $_->name } @set_fields;
        push(@set_fields, qw(short_desc version reporter));
        foreach my $key (keys %values) {
            delete $values{$key} unless grep { $_ eq $key } @set_fields;
        }
    }

    $product->update();

    return \%values;
}

# Flags
sub _create_flags {
    my ($number, $setter, $requestee) = @_;

    my $flagtypes = _create_flagtypes($number);

    my %flags;
    foreach my $type (qw(a b)) {
        $flags{$type} = _get_flag_values(@_, $flagtypes->{$type});
    }
    return \%flags;
}

sub _create_flagtypes {
    my ($number) = @_;
    my $dbh = Bugzilla->dbh;
    my $name = "$number-flag-" . random();
    my $desc = "FlagType $number";

    my %flagtypes; 
    foreach my $target (qw(a b)) {
         $dbh->do("INSERT INTO flagtypes
                  (name, description, target_type, is_requestable, 
                   is_requesteeble, is_multiplicable, cc_list)
                   VALUES (?,?,?,1,1,1,'')",
                   undef, $name, $desc, $target);
         my $id = $dbh->bz_last_key('flagtypes', 'id');
         $dbh->do('INSERT INTO flaginclusions (type_id) VALUES (?)',
                  undef, $id);
         my $flagtype = new Bugzilla::FlagType($id);
         $flagtypes{$target} = $flagtype;
    }
    return \%flagtypes;
}

sub _get_flag_values {
    my ($number, $setter, $requestee, $flagtype) = @_;

    my @set_flags;
    if ($number <= 2) {
        foreach my $value (qw(? - + ?)) {
            my $flag = { type_id => $flagtype->id, status => $value,
                         setter => $setter, flagtype => $flagtype };
            push(@set_flags, $flag);
        }
        $set_flags[0]->{requestee} = $requestee->login;
    }
    else {
        @set_flags = ({ type_id => $flagtype->id, status => '+',
                        setter => $setter, flagtype => $flagtype });
    }
    return \@set_flags;
}

# Attachments
sub _get_attach_values {
    my ($number, $for_create) = @_;

    my $boolean = $number == 1 ? 1 : 0;
    if ($for_create) {
        $boolean = !$boolean ? 1 : 0;
    }
    my $ispatch = $for_create ? 'ispatch' : 'is_patch';
    my $isobsolete = $for_create ? 'isobsolete' : 'is_obsolete';
    my $isprivate = $for_create ? 'isprivate' : 'is_private';
    my $mimetype = $for_create ? 'mimetype' : 'content_type';

    my %values = (
        description => "$number-attach_desc-" . random(),
        filename => "$number-filename-" . random(),
        $ispatch => $boolean,
        $isobsolete => $boolean,
        $isprivate => $boolean,
        $mimetype => "text/x-$number-" . random(),
    );
    if ($for_create) {
        $values{data} = "$number-data-" . random() . random();
    }
    return \%values;
}

################
# Bug Creation #
################

sub _create_one_bug {
    my ($self, $number) = @_;
    my $dbh = Bugzilla->dbh;
    
    # We need bug 6 to have a unique alias that is not a clone of bug 1's,
    # so we get the alias separately from the other parameters.
    my $alias = $self->bug_create_value($number, 'alias');
    my $update_alias = $self->bug_update_value($number, 'alias');
    
    # Otherwise, make bug 6 a clone of bug 1.
    my $real_number = $number;
    $number = 1 if $number == 6;
    
    my $reporter = $self->bug_create_value($number, 'reporter');
    Bugzilla->set_user(Bugzilla::User->check($reporter));
    
    # We create the bug with one set of values, and then we change it
    # to have different values.
    my %params = %{ $self->_bug_create_values->{$number} };
    $params{alias} = $alias;
    
    # There are some things in bug_create_values that shouldn't go into
    # create().
    delete @params{qw(attachment set_flags tag)};
    
    my ($status, $resolution, $see_also) = 
        delete @params{qw(bug_status resolution see_also)};
    # All the bugs are created with everconfirmed = 0.
    $params{bug_status} = 'UNCONFIRMED';
    my $bug = Bugzilla::Bug->create(\%params);
    
    # These are necessary for the changedfrom tests.
    my $extra_values = $self->_extra_bug_create_values->{$number};
    foreach my $field (qw(comments remaining_time percentage_complete
                         keyword_objects everconfirmed dependson blocked
                         groups_in classification actual_time))
    {
        $extra_values->{$field} = $bug->$field;
    }
    $extra_values->{reporter_accessible} = $number == 1 ? 0 : 1;
    $extra_values->{cclist_accessible}   = $number == 1 ? 0 : 1;
    
    if ($number == 5) {
        # Bypass Bugzilla::Bug--we don't want any changes in bugs_activity
        # for bug 5.
        $dbh->do('UPDATE bugs SET qa_contact = NULL, reporter_accessible = 0,
                                  cclist_accessible = 0 WHERE bug_id = ?',
                 undef, $bug->id);
        $dbh->do('DELETE FROM cc WHERE bug_id = ?', undef, $bug->id);
        my $ts = '1970-01-01 00:00:00';
        $dbh->do('UPDATE bugs SET creation_ts = ?, delta_ts = ?
                   WHERE bug_id = ?', undef, $ts, $ts, $bug->id);
        $dbh->do('UPDATE longdescs SET bug_when = ? WHERE bug_id = ?',
                 undef, $ts, $bug->id);
        $bug->{creation_ts} = $ts;
        $extra_values->{see_also} = [];
    }
    else {
        # Manually set the creation_ts so that each bug has a different one.
        #
        # Also, manually update the resolution and bug_status, because
        # we want to see both of them change in bugs_activity, so we
        # have to start with values for both (and as of the time when I'm
        # writing this test, Bug->create doesn't support setting resolution).
        #
        # Same for see_also.
        my $timestamp = timestamp($number, $number - 1);
        my $creation_ts = $timestamp->ymd . ' ' . $timestamp->hms;
        $bug->{creation_ts} = $creation_ts;
        $dbh->do('UPDATE longdescs SET bug_when = ? WHERE bug_id = ?',
                 undef, $creation_ts, $bug->id);
        $dbh->do('UPDATE bugs SET creation_ts = ?, bug_status = ?,
                  resolution = ? WHERE bug_id = ?',
                 undef, $creation_ts, $status, $resolution, $bug->id);
        $dbh->do('INSERT INTO bug_see_also (bug_id, value, class) VALUES (?,?,?)',
                 undef, $bug->id, $see_also, 'Bugzilla::BugUrl::Bugzilla');
        $extra_values->{see_also} = $bug->see_also;

        # All the tags must be created as the admin user, so that the
        # admin user can find them, later.
        my $original_user = Bugzilla->user;
        Bugzilla->set_user($self->admin);
        my $tags = $self->bug_create_value($number, 'tag');
        $bug->add_tag($_) foreach @$tags;
        $extra_values->{tags} = $tags;
        Bugzilla->set_user($original_user);

        if ($number == 1) {
            # Bug 1 needs to start off with reporter_accessible and
            # cclist_accessible being 0, so that when we change them to 1,
            # that change shows up in bugs_activity.
            $dbh->do('UPDATE bugs SET reporter_accessible = 0,
                      cclist_accessible = 0 WHERE bug_id = ?',
                      undef, $bug->id);
            # Bug 1 gets three comments, so that longdescs.count matches it
            # uniquely. The third comment is added in the middle, so that the
            # last comment contains all of the important data, like work_time.
            $bug->add_comment("1-comment-" . random(100));
        }
        
        my %update_params = %{ $self->_bug_update_values->{$number} };
        my %reverse_map = reverse %{ Bugzilla::Bug->FIELD_MAP };
        foreach my $db_name (keys %reverse_map) {
            next if $db_name eq 'comment';
            next if $db_name eq 'status_whiteboard';
            if (exists $update_params{$db_name}) {
                my $update_name = $reverse_map{$db_name};
                $update_params{$update_name} = delete $update_params{$db_name};
            }
        }
        
        my ($new_status, $new_res) = 
            delete @update_params{qw(status resolution)};
        # Bypass the status workflow.
        $bug->{bug_status} = $new_status;
        $bug->{resolution} = $new_res;
        $bug->{everconfirmed} = 1 if $number == 1;
        
        # add/remove/set fields.
        $update_params{keywords} = { set => $update_params{keywords} };
        $update_params{groups} = { add => $update_params{groups},
                                   remove => $bug->groups_in };
        my @cc_remove = map { $_->login } @{ $bug->cc_users };
        my $cc_new = $update_params{cc};
        my @cc_add = ref($cc_new) ? @$cc_new : ($cc_new);
        # We make the admin an explicit CC on bug 1 (but not on bug 6), so
        # that we can test the %user% pronoun properly.
        if ($real_number == 1) {
            push(@cc_add, $self->admin->login);
        }
        $update_params{cc} = { add => \@cc_add, remove => \@cc_remove };
        my $see_also_remove = $bug->see_also;
        my $see_also_add = [$update_params{see_also}];
        $update_params{see_also} = { add => $see_also_add, 
                                     remove => $see_also_remove };
        $update_params{comment} = { body => $update_params{comment} };
        $update_params{work_time} = $number;
        # Setting work_time kills the remaining_time, so we need to
        # preserve that. We add 8 because that produces an integer
        # percentage_complete for bug 1, which is necessary for
        # accurate "equals"-type searching.
        $update_params{remaining_time} = $number + 8;
        $update_params{reporter_accessible} = $number == 1 ? 1 : 0;
        $update_params{cclist_accessible} = $number == 1 ? 1 : 0;
        $update_params{alias} = $update_alias;

        $bug->set_all(\%update_params);
        my $flags = $self->bug_create_value($number, 'set_flags')->{b};
        $bug->set_flags([], $flags);
        $timestamp->set(second => $number);
        $bug->update($timestamp->ymd . ' ' . $timestamp->hms);
        $extra_values->{flags} = $bug->flags;
        
        # It's not generally safe to do update() multiple times on
        # the same Bug object.
        $bug = new Bugzilla::Bug($bug->id);
        my $update_flags = $self->bug_update_value($number, 'set_flags')->{b};
        $_->{status} = 'X' foreach @{ $bug->flags };
        $bug->set_flags($bug->flags, $update_flags);
        if ($number == 1) {
            my $comment_id = $bug->comments->[-1]->id;
            $bug->set_comment_is_private({ $comment_id => 1 });
        }
        $bug->update($bug->delta_ts);
        
        my $attach_create = $self->bug_create_value($number, 'attachment');
        my $attachment = Bugzilla::Attachment->create({
            bug => $bug,
            creation_ts => $creation_ts,
            %$attach_create });
        # Store for the changedfrom tests.
        $extra_values->{attachments} = 
            [new Bugzilla::Attachment($attachment->id)];
        
        my $attach_update = $self->bug_update_value($number, 'attachment');
        $attachment->set_all($attach_update);
        # In order to keep the mimetype on the ispatch attachment,
        # we need to bypass the validator.
        $attachment->{mimetype} = $attach_update->{content_type};
        my $attach_flags = $self->bug_update_value($number, 'set_flags')->{a};
        $attachment->set_flags([], $attach_flags);
        $attachment->update($bug->delta_ts);
    }
    
    # Values for changedfrom.
    $extra_values->{creation_ts} = $bug->creation_ts;
    $extra_values->{delta_ts}    = $bug->creation_ts;
    
    return new Bugzilla::Bug($bug->id);
}

###################################
# Test::Builder Memory Efficiency #
###################################

# Test::Builder stores information for each test run, but Test::Harness
# and TAP::Harness don't actually need this information. When we run 60
# million tests, the history eats up all our memory. (After about
# 1 million tests, memory usage is around 1 GB.)
#
# The only part of the history that Test::More actually *uses* is the "ok"
# field, which we store more efficiently, in an array, and then we re-populate
# the Test_Results in Test::Builder at the end of the test.
sub clean_test_history {
    my ($self) = @_;
    return if !$self->option('long');
    my $builder = Test::More->builder;
    my $current_test = $builder->current_test;

    # I don't use details() because I don't want to copy the array.
    my $results = $builder->{Test_Results};
    my $check_test = $current_test - 1;
    while (my $result = $results->[$check_test]) {
        last if !$result;
        $self->test_success($check_test, $result->{ok});
        $check_test--;
    }

    # Truncate the test history array, but retain the current test number.
    $builder->{Test_Results} = [];
    $builder->{Curr_Test} = $current_test;
}

sub test_success {
    my ($self, $index, $status) = @_;
    $self->{test_success}->[$index] = $status;
    return $self->{test_success};
}

sub repopulate_test_results {
    my ($self) = @_;
    return if !$self->option('long');
    $self->clean_test_history();
    # We create only two hashes, for memory efficiency.
    my %ok = ( ok => 1 );
    my %not_ok = ( ok => 0 );
    my @results;
    foreach my $success (@{ $self->{test_success} }) {
        push(@results, $success ? \%ok : \%not_ok);
    }
    my $builder = Test::More->builder;
    $builder->{Test_Results} = \@results;
}

##########
# Caches #
##########

# When doing AND and OR tests, we essentially test the same field/operator
# combinations over and over. So, if we're going to be running those tests,
# we cache the translated_value of the FieldTests globally so that we don't
# have to re-run the value-translation code every time (which can be pretty
# slow).
sub value_translation_cache {
    my ($self, $field_test, $value) = @_;
    return if !$self->option('long');
    my $test_name = $field_test->name;
    if (@_ == 3) {
        $self->{value_translation_cache}->{$test_name} = $value;
    }
    return $self->{value_translation_cache}->{$test_name};
}

# When doing AND/OR tests, the value for transformed_value_was_equal
# (see Bugzilla::Test::Search::FieldTest) won't be recalculated
# if we pull our values from the value_translation_cache. So we need
# to also cache the values for transformed_value_was_equal.
sub was_equal_cache {
    my ($self, $field_test, $number, $value) = @_;
    return if !$self->option('long');
    my $test_name = $field_test->name;
    if (@_ == 4) {
        $self->{tvwe_cache}->{$test_name}->{$number} = $value;
    }
    return $self->{tvwe_cache}->{$test_name}->{$number};
}

#############
# Main Test #
#############

sub run {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;

    # We want backtraces on any "die" message or any warning.
    # Otherwise it's hard to trace errors inside of Bugzilla::Search from
    # reading automated test run results.
    local $SIG{__WARN__} = \&Carp::cluck;
    local $SIG{__DIE__}  = \&Carp::confess;

    $dbh->bz_start_transaction();
    
    # Some parameters need to be set in order for the tests to function
    # properly.
    my $everybody = $self->everybody;
    my $params = Bugzilla->params;
    local $params->{'useclassification'} = 1;
    local $params->{'useqacontact'} = 1;
    local $params->{'usetargetmilestone'} = 1;
    local $params->{'mail_delivery_method'} = 'None';
    local $params->{'timetrackinggroup'} = $everybody->name;
    local $params->{'insidergroup'} = $everybody->name;

    $self->_setup_bugs();
    
    # Even though _setup_bugs set us as an admin, we want to be sure at
    # this point that we have an admin with refreshed group memberships.
    Bugzilla->set_user($self->admin);
    foreach my $test (CUSTOM_SEARCH_TESTS) {
        my $custom_test = new Bugzilla::Test::Search::CustomTest($test, $self);
        $custom_test->run();
    }
    foreach my $test (SPECIAL_PARAM_TESTS) {
        my $operator_test =
            new Bugzilla::Test::Search::OperatorTest($test->{operator}, $self);
        my $field = Bugzilla::Field->check($test->{field});
        my $special_test = new Bugzilla::Test::Search::FieldTestNormal(
            $operator_test, $field, $test);
        $special_test->run();
    }
    foreach my $operator ($self->top_level_operators) {
        my $operator_test =
            new Bugzilla::Test::Search::OperatorTest($operator, $self);
        $operator_test->run();
    }

    # Rollbacks won't get rid of bugs_fulltext entries, so we do that ourselves.
    my @bug_ids = map { $_->id } $self->bugs;
    my $bug_id_string = join(',', @bug_ids);
    $dbh->do("DELETE FROM bugs_fulltext WHERE bug_id IN ($bug_id_string)");
    $dbh->bz_rollback_transaction();
    $self->repopulate_test_results();
}

# This makes a few changes to the bugs after they're created--changes
# that can only be done after all the bugs have been created.
sub _setup_bugs {
    my ($self) = @_;
    $self->_setup_dependencies();
    $self->_set_bug_id_fields();
    $self->_protect_bug_6();
}
sub _setup_dependencies {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    
    # Set up depedency relationships between the bugs.
    # Bug 1 + 6 depend on bug 2 and block bug 3.
    my $bug2 = $self->bug(2);
    my $bug3 = $self->bug(3);
    foreach my $number (1,6) {
        my $bug = $self->bug($number);
        my @original_delta = ($bug2->delta_ts, $bug3->delta_ts);
        Bugzilla->set_user($bug->reporter);
        $bug->set_dependencies([$bug2->id], [$bug3->id]);
        $bug->update($bug->delta_ts);
        # Setting dependencies changed the delta_ts on bug2 and bug3, so
        # re-set them back to what they were before. However, we leave
        # the correct update times in bugs_activity, so that the changed*
        # searches still work right.
        my $set_delta = $dbh->prepare(
            'UPDATE bugs SET delta_ts = ? WHERE bug_id = ?');
        foreach my $row ([$original_delta[0], $bug2->id], 
                         [$original_delta[1], $bug3->id])
        {
            $set_delta->execute(@$row);
        }
    }
}

sub _set_bug_id_fields {
    my ($self) = @_;
    # BUG_ID fields couldn't be set before, because before we create bug 1,
    # we don't necessarily have any valid bug ids.)
    my @bug_id_fields = grep { $_->type == FIELD_TYPE_BUG_ID }
                             $self->all_fields;
    foreach my $number (1..NUM_BUGS) {
        my $bug = $self->bug($number);
        $number = 1 if $number == 6;
        next if $number == 5;
        my $other_bug = $self->bug($number + 1);
        Bugzilla->set_user($bug->reporter);
        foreach my $field (@bug_id_fields) {
            $bug->set_custom_field($field, $other_bug->id);
            $bug->update($bug->delta_ts);
        }
    }
}

sub _protect_bug_6 {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    
    Bugzilla->set_user($self->admin);
    
    # Put bug6 in the nobody group.
    my $nobody = $self->nobody;
    # We pull it newly from the DB to be sure it's safe to call update()
    # on.
    my $bug6 = new Bugzilla::Bug($self->bug(6)->id);
    $bug6->add_group($nobody);
    $bug6->update($bug6->delta_ts);
    
    # Remove the admin (and everybody else) from the $nobody group.
    $dbh->do('DELETE FROM group_group_map 
               WHERE grantor_id = ? OR member_id = ?', undef,
             $nobody->id, $nobody->id);
}

1;
