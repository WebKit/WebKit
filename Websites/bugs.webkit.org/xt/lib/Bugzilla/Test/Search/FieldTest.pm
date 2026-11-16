# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module represents the tests that get run on a single
# operator/field combination for Bugzilla::Test::Search.
# This is where all the actual testing happens.
package Bugzilla::Test::Search::FieldTest;

use strict;
use warnings;
use Bugzilla::Search;
use Bugzilla::Test::Search::Constants;
use Bugzilla::Util qw(trim);

use Data::Dumper;
use Scalar::Util qw(blessed);
use Test::More;
use Test::Exception;

###############
# Constructor #
###############

sub new {
    my ($class, $operator_test, $field, $test) = @_;
    return bless { operator_test => $operator_test,
                   field_object => $field,
                   raw_test     => $test }, $class;
}

#############
# Accessors #
#############

sub num_tests { return TESTS_PER_RUN }

# The Bugzilla::Test::Search::OperatorTest that this is a child of.
sub operator_test { return $_[0]->{operator_test} }
# The Bugzilla::Field being tested.
sub field_object { return $_[0]->{field_object} }
# The name of the field being tested, which we need much more often
# than we need the object.
sub field {
    my ($self) = @_;
    $self->{field_name} ||= $self->field_object->name;
    return $self->{field_name};
}
# The Bugzilla::Test::Search object that this is a child of.
sub search_test { return $_[0]->operator_test->search_test }
# The operator being tested
sub operator { return $_[0]->operator_test->operator }
# The bugs currently being tested by Bugzilla::Test::Search.
sub bugs { return $_[0]->search_test->bugs }
sub bug {
    my $self = shift;
    return $self->search_test->bug(@_);
}
sub number {
    my ($self, $id) = @_;
    foreach my $number (1..NUM_BUGS) {
        return $number if $self->search_test->bug($number)->id == $id;
    }
    return 0;
}

# The name displayed for this test by Test::More. Used in test descriptions.
sub name {
    my ($self) = @_;
    my $field = $self->field;
    my $operator = $self->operator;
    my $value = $self->main_value;
    
    my $name = "$field-$operator-$value";
    if (my $extra_name = $self->test->{extra_name}) {
        $name .= "-$extra_name";
    }
    return $name;
}

# The appropriate value from the TESTS constant for this test, taking
# into account overrides.
sub test {
    my $self = shift;
    return $self->{test} if $self->{test};
    
    my %test = %{ $self->{raw_test} };
    
    # We have field name overrides...
    my $override = $test{override}->{$self->field};
    # And also field type overrides.
    if (!$override) {
        $override = $test{override}->{$self->field_object->type} || {};
    }
    
    foreach my $key (%$override) {
        $test{$key} = $override->{$key};
    }
    
    $self->{test} = \%test;
    return $self->{test};
}

# All the values for all the bugs for this field.
sub _field_values {
    my ($self) = @_;
    return $self->{field_values} if $self->{field_values};
    
    my %field_values;
    foreach my $number (1..NUM_BUGS) {
        $field_values{$number} = $self->_field_values_for_bug($number);
    }
    $self->{field_values} = \%field_values;
    return $self->{field_values};
}
# The values for this field for the numbered bug.
sub bug_values {
    my ($self, $number) = @_;
    return @{ $self->_field_values->{$number} };
}

# The untranslated, non-overriden value--used in the name of the test
# and other places.
sub main_value { return $_[0]->{raw_test}->{value} }
# The untranslated test value, taking into account overrides.
sub test_value { return $_[0]->test->{value} };
# The value translated appropriately for passing to Bugzilla::Search.
sub translated_value {
    my $self = shift;
    if (!exists $self->{translated_value}) {
        my $value = $self->search_test->value_translation_cache($self);
        if (!defined $value) {
            $value = $self->_translate_value();
            $self->search_test->value_translation_cache($self, $value);
        }
        $self->{translated_value} = $value;
    }
    return $self->{translated_value};
}
# Used in failure diagnostic messages.
sub debug_fail {
    my ($self, $number, $results, $sql) = @_;
    my @expected = @{ $self->test->{contains} };
    my @results = sort
                  map { $self->number($_) }
                  map { $_->[0] }
                  @$results;
    return
        "   Value: '" . $self->translated_value . "'\n" .
        "Expected: [" . join(',', @expected) . "]\n" .
        " Results: [" . join(',', @results) . "]\n" .
        trim($sql) . "\n";
}

# True for a bug if we ran the "transform" function on it and the
# result was equal to its first value.
sub transformed_value_was_equal {
    my ($self, $number, $value) = @_;
    if (@_ > 2) {
        $self->{transformed_value_was_equal}->{$number} = $value;
        $self->search_test->was_equal_cache($self, $number, $value);
    }
    my $cached = $self->search_test->was_equal_cache($self, $number);
    return $cached if defined $cached;
    return $self->{transformed_value_was_equal}->{$number};
}

# True if this test is supposed to contain the numbered bug.
sub bug_is_contained {
    my ($self, $number) = @_;
    my $contains = $self->test->{contains};
    if ($self->transformed_value_was_equal($number)
        and !$self->test->{override}->{$self->field}->{contains})
    {
        $contains = $self->test->{if_equal}->{contains};
    }
    return grep($_ == $number, @$contains) ? 1 : 0;
}

###################################################
# Accessors: Ways of doing SKIP and TODO on tests #
###################################################

# The tests we know are broken for this operator/field combination.
sub _known_broken {
    my ($self, $constant, $skip_pg_check) = @_;

    $constant ||= KNOWN_BROKEN;
    my $field = $self->field;
    my $type = $self->field_object->type;
    my $operator = $self->operator;
    my $value = $self->main_value;
    my $value_name = "$operator-$value";
    if (my $extra_name = $self->test->{extra_name}) {
        $value_name .= "-$extra_name";
    }

    my $value_broken = $constant->{$value_name}->{$field};
    $value_broken ||= $constant->{$value_name}->{$type};
    return $value_broken if $value_broken;
    my $operator_broken = $constant->{$operator}->{$field};
    $operator_broken ||= $constant->{$operator}->{$type};
    return $operator_broken if $operator_broken;
    return {};
}

# True if the "contains" search for the numbered bug is broken.
# That is, either the result is supposed to contain it and doesn't,
# or the result is not supposed to contain it and does.
sub contains_known_broken {
    my ($self, $number) = @_;
    my $field = $self->field;
    my $operator = $self->operator;

    my $contains_broken = $self->_known_broken->{contains} || [];
    if (grep($_ == $number, @$contains_broken)) {
        return "$field $operator contains $number is known to be broken";
    }
    return undef;
}

# Used by subclasses. Checks both bug_is_contained and contains_known_broken
# to tell you whether or not the bug will *actually* be found by the test.
sub will_actually_contain_bug {
    my ($self, $number) = @_;
    my $is_contained = $self->bug_is_contained($number) ? 1 : 0;
    my $is_broken = $self->contains_known_broken($number) ? 1 : 0;

    # If the test is supposed to contain the bug and *isn't* broken,
    # then the test will contain the bug.
    return 1 if ($is_contained and !$is_broken);
    # If this test is *not* supposed to contain the bug, but that test is
    # broken, then this test *will* contain the bug.
    return 1 if (!$is_contained and $is_broken);

    return 0;
}

# Returns a string if creating a Bugzilla::Search object throws an error,
# with this field/operator/value combination.
sub search_known_broken {
    my ($self) = @_;
    my $field = $self->field;
    my $operator = $self->operator;
    if ($self->_known_broken->{search}) {
        return "Bugzilla::Search for $field $operator is known to be broken";
    }
    return undef;
}
    
# Returns a string if we haven't yet implemented the tests for this field,
# but we plan to in the future.
sub field_not_yet_implemented {
    my ($self) = @_;
    my $skip_this_field = grep { $_ eq $self->field } SKIP_FIELDS;
    if ($skip_this_field) {
        my $field = $self->field;
        return "$field testing not yet implemented";
    }
    return undef;
}

# Returns a message if this field/operator combination can't ever be run.
# At no time in the future will this field/operator combination ever work.
sub invalid_field_operator_combination {
    my ($self) = @_;
    my $field = $self->field;
    my $operator = $self->operator;
    
    if ($field eq 'content' && $operator !~ /matches/) {
        return "content field does not support $operator";
    }
    elsif ($operator =~ /matches/ && $field ne 'content') {
        return "matches operator does not support fields other than content";
    }
    return undef;
}

# True if this field is broken in an OR combination.
sub join_broken {
    my ($self, $or_broken_map) = @_;
    my $or_broken = $or_broken_map->{$self->field . '-' . $self->operator};
    if (!$or_broken) {
        # See if this is a comment field, and in that case, if there's
        # a generic entry for all comment fields.
        my $is_comment_field = COMMENT_FIELDS->{$self->field};
        if ($is_comment_field) {
            $or_broken = $or_broken_map->{'longdescs.-' . $self->operator};
        }
    }
    return $or_broken;
}

#########################################
# Accessors: Bugzilla::Search Arguments #
#########################################

# The data that will get passed to Bugzilla::Search as its arguments.
sub search_params {
    my ($self) = @_;
    return $self->{search_params} if $self->{search_params};

    my %params = (
        "field0-0-0" => $self->field,
        "type0-0-0"  => $self->operator,
        "value0-0-0"  => $self->translated_value,
    );
    
    $self->{search_params} = \%params;
    return $self->{search_params};
}

sub search_columns {
    my ($self) = @_;
    my $field = $self->field;
    my @search_fields = qw(bug_id);
    if ($self->field_object->buglist) {
        my $col_name = COLUMN_TRANSLATION->{$field} || $field;
        push(@search_fields, $col_name);
    }
    return \@search_fields;
}


################
# Field Values #
################

sub _field_values_for_bug {
    my ($self, $number) = @_;
    my $field = $self->field;

    my @values;

    if ($field =~ /^attach.+\.(.+)$/ ) {
        my $attach_field = $1;
        $attach_field = ATTACHMENT_FIELDS->{$attach_field} || $attach_field;
        @values = $self->_values_for($number, 'attachments', $attach_field);
    }
    elsif (my $flag_field = FLAG_FIELDS->{$field}) {
        @values = $self->_values_for($number, 'flags', $flag_field);
    }
    elsif (my $translation = COMMENT_FIELDS->{$field}) {
        @values = $self->_values_for($number, 'comments', $translation);
        # We want the last value to come first, so that single-value
        # searches use the last comment.
        @values = reverse @values;
    }
    elsif ($field eq 'longdescs.count') {
        @values = scalar(@{ $self->bug($number)->comments });
    }
    elsif ($field eq 'work_time') {
        @values = $self->_values_for($number, 'actual_time');
    }
    elsif ($field eq 'bug_group') {
        @values = $self->_values_for($number, 'groups_in', 'name');
    }
    elsif ($field eq 'keywords') {
        @values = $self->_values_for($number, 'keyword_objects', 'name'); 
    }
    elsif ($field eq 'content') {
        @values = $self->_values_for($number, 'short_desc');
    }
    elsif ($field eq 'see_also') {
        @values = $self->_values_for($number, 'see_also', 'name');
    }
    elsif ($field eq 'tag') {
        @values = $self->_values_for($number, 'tags');
    }
    # Bugzilla::Bug truncates creation_ts, but we need the full value
    # from the database. This has no special value for changedfrom,
    # because it never changes.
    elsif ($field eq 'creation_ts') {
        my $bug = $self->bug($number);
        my $creation_ts = Bugzilla->dbh->selectrow_array(
            'SELECT creation_ts FROM bugs WHERE bug_id = ?',
            undef, $bug->id);
        @values = ($creation_ts);
    }
    else {
        @values = $self->_values_for($number, $field);
    }

    # We convert user objects to their login name, here, all in one
    # block for simplicity.
    if (grep { $_ eq $field } USER_FIELDS) {
        # requestees.login_name is empty for most bugs (but checking
        # blessed(undef) handles that.
        # Values that come from %original_values aren't User objects.
        @values = map { blessed($_) ? $_->login : $_ } @values;
        @values = grep { defined $_ } @values;
    }
    
    return \@values;
}

sub _values_for {
    my ($self, $number, $bug_field, $item_field) = @_;

    my $item;
    if ($self->operator eq 'changedfrom') {
        $item = $self->search_test->bug_create_value($number, $bug_field);
    }
    else {
        my $bug = $self->bug($number);
        $item = $bug->$bug_field;
    }

    if ($item_field) {
        if ($bug_field eq 'flags' and $item_field eq 'name') {
            return (map { $_->name . $_->status } @$item);
        }
        return (map { $self->_get_item($_, $item_field) } @$item);
    }

    return @$item if ref($item) eq 'ARRAY';
    return $item if defined $item;
    return ();
}

sub _get_item {
    my ($self, $from, $field) = @_;
    if (blessed($from)) {
        return $from->$field;
    }
    return $from->{$field};
}

#####################
# Value Translation #
#####################

# This function translates the "value" specified in TESTS into an actual
# search value to pass to Search.pm. This means that we get the value
# from the current bug (or, in the case of changedfrom, from %original_values)
# and then we insert it as required into the "value" from TESTS. (For example,
# <1> becomes the value for the field from bug 1.)
sub _translate_value {
    my $self = shift;
    my $value = $self->test_value;
    foreach my $number (1..NUM_BUGS) {
        $value = $self->_translate_value_for_bug($number, $value);
    }
    # Sanity check to make sure that none of the <> stuff was left in.
    if ($value =~ /<\d/) {
        die $self->name . ": value untranslated: $value\n";
    }
    return $value;
}

sub _translate_value_for_bug {
    my ($self, $number, $value) = @_;
    
    my $bug = $self->bug($number);
    
    my $bug_id = $bug->id;
    $value =~ s/<$number-id>/$bug_id/g;
    my $bug_delta = $bug->delta_ts;
    $value =~ s/<$number-delta>/$bug_delta/g;
    my $reporter = $bug->reporter->login;
    $value =~ s/<$number-reporter>/$reporter/g;
    if ($value =~ /<$number-bug_group>/) {
        my @bug_groups = map { $_->name } @{ $bug->groups_in };
        @bug_groups = grep { $_ =~ /^\d+-group-/ } @bug_groups;
        my $group = $bug_groups[0];
        $value =~ s/<$number-bug_group>/$group/g;
    }
    
    my @bug_values = $self->bug_values($number);    
    return $value if !@bug_values;
    
    if ($self->operator =~ /substr/) {
        @bug_values = map { $self->_substr_value($_) } @bug_values;
    }

    my $string_value = $bug_values[0];
    if ($self->operator =~ /word/) {
        $string_value = join(' ', @bug_values);
    }
    if (my $func = $self->test->{transform}) {
        my $transformed = $func->(@bug_values);
        my $is_equal = $transformed eq $bug_values[0] ? 1 : 0;
        $self->transformed_value_was_equal($number, $is_equal);
        $string_value = $transformed;
    }

    if ($self->test->{escape}) {
        $string_value = quotemeta($string_value);
    }
    $value =~ s/<$number>/$string_value/g;
    
    return $value;
}

sub _substr_value {
    my ($self, $value) = @_;
    my $field = $self->field;
    my $type  = $self->field_object->type;
    my $substr_size = SUBSTR_SIZE;
    if (exists FIELD_SUBSTR_SIZE->{$field}) {
        $substr_size = FIELD_SUBSTR_SIZE->{$field};
    }
    elsif (exists FIELD_SUBSTR_SIZE->{$type}) {
        $substr_size = FIELD_SUBSTR_SIZE->{$type};
    }
    if ($substr_size > 0) {
        # The field name is included in every field value, and if it's
        # long, it might take up the whole substring, and we don't want that.
        if (!grep { $_ eq $field or $_ eq $type } SUBSTR_NO_FIELD_ADD) {
            $substr_size += length($field);
        }
        my $string = substr($value, 0, $substr_size);
        return $string;
    }
    return substr($value, $substr_size);
}

#####################
# Main Test Methods #
#####################

sub run {
    my ($self) = @_;
    
    my $invalid_combination = $self->invalid_field_operator_combination;
    my $field_not_implemented = $self->field_not_yet_implemented;

    SKIP: {    
        skip($invalid_combination, $self->num_tests) if $invalid_combination;
        TODO: {
            todo_skip ($field_not_implemented, $self->num_tests) if $field_not_implemented;
            $self->do_tests();
        }
    }
}

sub do_tests {
    my ($self) = @_;
    my $name = $self->name;

    my $search_broken = $self->search_known_broken;
    
    my $search = $self->_test_search_object_creation();

    my $sql;
    TODO: {
        local $TODO = $search_broken if $search_broken;
        lives_ok { $sql = $search->_sql } "$name: generate SQL";
    }
    
    my $results;
    SKIP: {
        skip "Can't run SQL without any SQL", 1 if !defined $sql;
        $results = $self->_test_sql($search);
    }

    $self->_test_content($results, $sql);
}

sub _test_search_object_creation {
    my ($self) = @_;
    my $name = $self->name;
    my @args = (fields => $self->search_columns, params => $self->search_params);
    my $search;
    lives_ok { $search = new Bugzilla::Search(@args) }
             "$name: create search object";
    return $search;
}

sub _test_sql {
    my ($self, $search) = @_;
    my $name = $self->name;
    my $results;
    lives_ok { $results = $search->data } "$name: Run SQL Query"
        or diag($search->_sql);
    return $results;
}

sub _test_content {
    my ($self, $results, $sql) = @_;

    SKIP: {
        skip "Without results we can't test them", NUM_BUGS if !$results;
        foreach my $number (1..NUM_BUGS) {
            $self->_test_content_for_bug($number, $results, $sql);
        }
    }
}

sub _test_content_for_bug {
    my ($self, $number, $results, $sql) = @_;
    my $name = $self->name;
    
    my $contains_known_broken = $self->contains_known_broken($number);
    
    my %result_ids = map { $_->[0] => 1 } @$results;
    my $bug_id = $self->bug($number)->id;
    
    TODO: {
        local $TODO = $contains_known_broken if $contains_known_broken;
        if ($self->bug_is_contained($number)) {
            ok($result_ids{$bug_id},
               "$name: contains bug $number ($bug_id)")
                or diag $self->debug_fail($number, $results, $sql);
        }
        else {
            ok(!$result_ids{$bug_id},
               "$name: does not contain bug $number ($bug_id)")
                or diag $self->debug_fail($number, $results, $sql);
        }
    }
}

1;
