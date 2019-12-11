# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

=head1 NAME

Bugzilla::Field - a particular piece of information about bugs
                  and useful routines for form field manipulation

=head1 SYNOPSIS

  use Bugzilla;
  use Data::Dumper;

  # Display information about all fields.
  print Dumper(Bugzilla->fields());

  # Display information about non-obsolete custom fields.
  print Dumper(Bugzilla->active_custom_fields);

  use Bugzilla::Field;

  # Display information about non-obsolete custom fields.
  # Bugzilla->fields() is a wrapper around Bugzilla::Field->get_all(),
  # with arguments which filter the fields before returning them.
  print Dumper(Bugzilla->fields({ obsolete => 0, custom => 1 }));

  # Create or update a custom field or field definition.
  my $field = Bugzilla::Field->create(
    {name => 'cf_silly', description => 'Silly', custom => 1});

  # Instantiate a Field object for an existing field.
  my $field = new Bugzilla::Field({name => 'qacontact_accessible'});
  if ($field->obsolete) {
      say $field->description . " is obsolete";
  }

  # Validation Routines
  check_field($name, $value, \@legal_values, $no_warn);
  $fieldid = get_field_id($fieldname);

=head1 DESCRIPTION

Field.pm defines field objects, which represent the particular pieces
of information that Bugzilla stores about bugs.

This package also provides functions for dealing with CGI form fields.

C<Bugzilla::Field> is an implementation of L<Bugzilla::Object>, and
so provides all of the methods available in L<Bugzilla::Object>,
in addition to what is documented here.

=cut

package Bugzilla::Field;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter Bugzilla::Object);
@Bugzilla::Field::EXPORT = qw(check_field get_field_id get_legal_field_values);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use List::MoreUtils qw(any);

use Scalar::Util qw(blessed);

###############################
####    Initialization     ####
###############################

use constant IS_CONFIG => 1;

use constant DB_TABLE   => 'fielddefs';
use constant LIST_ORDER => 'sortkey, name';

use constant DB_COLUMNS => qw(
    id
    name
    description
    long_desc
    type
    custom
    mailhead
    sortkey
    obsolete
    enter_bug
    buglist
    visibility_field_id
    value_field_id
    reverse_desc
    is_mandatory
    is_numeric
);

use constant VALIDATORS => {
    custom       => \&_check_custom,
    description  => \&_check_description,
    long_desc    => \&_check_long_desc,
    enter_bug    => \&_check_enter_bug,
    buglist      => \&Bugzilla::Object::check_boolean,
    mailhead     => \&_check_mailhead,
    name         => \&_check_name,
    obsolete     => \&_check_obsolete,
    reverse_desc => \&_check_reverse_desc,
    sortkey      => \&_check_sortkey,
    type         => \&_check_type,
    value_field_id      => \&_check_value_field_id,
    visibility_field_id => \&_check_visibility_field_id,
    visibility_values => \&_check_visibility_values,
    is_mandatory => \&Bugzilla::Object::check_boolean,
    is_numeric   => \&_check_is_numeric,
};

use constant VALIDATOR_DEPENDENCIES => {
    is_numeric => ['type'],
    name => ['custom'],
    type => ['custom'],
    reverse_desc => ['type'],
    value_field_id => ['type'],
    visibility_values => ['visibility_field_id'],
};

use constant UPDATE_COLUMNS => qw(
    description
    long_desc
    mailhead
    sortkey
    obsolete
    enter_bug
    buglist
    visibility_field_id
    value_field_id
    reverse_desc
    is_mandatory
    is_numeric
    type
);

# How various field types translate into SQL data definitions.
use constant SQL_DEFINITIONS => {
    # Using commas because these are constants and they shouldn't
    # be auto-quoted by the "=>" operator.
    FIELD_TYPE_FREETEXT,      { TYPE => 'varchar(255)', 
                                NOTNULL => 1, DEFAULT => "''"},
    FIELD_TYPE_SINGLE_SELECT, { TYPE => 'varchar(64)', NOTNULL => 1,
                                DEFAULT => "'---'" },
    FIELD_TYPE_TEXTAREA,      { TYPE => 'MEDIUMTEXT', 
                                NOTNULL => 1, DEFAULT => "''"},
    FIELD_TYPE_DATETIME,      { TYPE => 'DATETIME'   },
    FIELD_TYPE_DATE,          { TYPE => 'DATE'       },
    FIELD_TYPE_BUG_ID,        { TYPE => 'INT3'       },
    FIELD_TYPE_INTEGER,       { TYPE => 'INT4',  NOTNULL => 1, DEFAULT => 0 },
};

# Field definitions for the fields that ship with Bugzilla.
# These are used by populate_field_definitions to populate
# the fielddefs table.
# 'days_elapsed' is set in populate_field_definitions() itself.
use constant DEFAULT_FIELDS => (
    {name => 'bug_id',       desc => 'Bug #',      in_new_bugmail => 1,
     buglist => 1, is_numeric => 1},
    {name => 'short_desc',   desc => 'Summary',    in_new_bugmail => 1,
     is_mandatory => 1, buglist => 1},
    {name => 'classification', desc => 'Classification', in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'product',      desc => 'Product',    in_new_bugmail => 1,
     is_mandatory => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'version',      desc => 'Version',    in_new_bugmail => 1,
     is_mandatory => 1, buglist => 1},
    {name => 'rep_platform', desc => 'Platform',   in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'bug_file_loc', desc => 'URL',        in_new_bugmail => 1,
     buglist => 1},
    {name => 'op_sys',       desc => 'OS/Version', in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'bug_status',   desc => 'Status',     in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'status_whiteboard', desc => 'Status Whiteboard',
     in_new_bugmail => 1, buglist => 1},
    {name => 'keywords',     desc => 'Keywords',   in_new_bugmail => 1,
     type => FIELD_TYPE_KEYWORDS, buglist => 1},
    {name => 'resolution',   desc => 'Resolution',
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'bug_severity', desc => 'Severity',   in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'priority',     desc => 'Priority',   in_new_bugmail => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'component',    desc => 'Component',  in_new_bugmail => 1,
     is_mandatory => 1,
     type => FIELD_TYPE_SINGLE_SELECT, buglist => 1},
    {name => 'assigned_to',  desc => 'AssignedTo', in_new_bugmail => 1,
     buglist => 1},
    {name => 'reporter',     desc => 'ReportedBy', in_new_bugmail => 1,
     buglist => 1},
    {name => 'qa_contact',   desc => 'QAContact',  in_new_bugmail => 1,
     buglist => 1},
    {name => 'assigned_to_realname',  desc => 'AssignedToName',
     in_new_bugmail => 0, buglist => 1},
    {name => 'reporter_realname',     desc => 'ReportedByName',
     in_new_bugmail => 0, buglist => 1},
    {name => 'qa_contact_realname',   desc => 'QAContactName',
     in_new_bugmail => 0, buglist => 1},
    {name => 'cc',           desc => 'CC',         in_new_bugmail => 1},
    {name => 'dependson',    desc => 'Depends on', in_new_bugmail => 1,
     is_numeric => 1, buglist => 1},
    {name => 'blocked',      desc => 'Blocks',     in_new_bugmail => 1,
     is_numeric => 1, buglist => 1},

    {name => 'attachments.description', desc => 'Attachment description'},
    {name => 'attachments.filename',    desc => 'Attachment filename'},
    {name => 'attachments.mimetype',    desc => 'Attachment mime type'},
    {name => 'attachments.ispatch',     desc => 'Attachment is patch',
     is_numeric => 1},
    {name => 'attachments.isobsolete',  desc => 'Attachment is obsolete',
     is_numeric => 1},
    {name => 'attachments.isprivate',   desc => 'Attachment is private',
     is_numeric => 1},
    {name => 'attachments.submitter',   desc => 'Attachment creator'},

    {name => 'target_milestone',      desc => 'Target Milestone',
     in_new_bugmail => 1, buglist => 1},
    {name => 'creation_ts',           desc => 'Creation date',
     buglist => 1},
    {name => 'delta_ts',              desc => 'Last changed date',
     buglist => 1},
    {name => 'longdesc',              desc => 'Comment'},
    {name => 'longdescs.isprivate',   desc => 'Comment is private',
     is_numeric => 1},
    {name => 'longdescs.count',       desc => 'Number of Comments',
     buglist => 1, is_numeric => 1},
    {name => 'alias',                 desc => 'Alias', buglist => 1},
    {name => 'everconfirmed',         desc => 'Ever Confirmed',
     is_numeric => 1},
    {name => 'reporter_accessible',   desc => 'Reporter Accessible',
     is_numeric => 1},
    {name => 'cclist_accessible',     desc => 'CC Accessible',
     is_numeric => 1},
    {name => 'bug_group',             desc => 'Group', in_new_bugmail => 1},
    {name => 'estimated_time',        desc => 'Estimated Hours',
     in_new_bugmail => 1, buglist => 1, is_numeric => 1},
    {name => 'remaining_time',        desc => 'Remaining Hours', buglist => 1,
     is_numeric => 1},
    {name => 'deadline',              desc => 'Deadline',
     type => FIELD_TYPE_DATETIME, in_new_bugmail => 1, buglist => 1},
    {name => 'commenter',             desc => 'Commenter'},
    {name => 'flagtypes.name',        desc => 'Flags', buglist => 1},
    {name => 'requestees.login_name', desc => 'Flag Requestee'},
    {name => 'setters.login_name',    desc => 'Flag Setter'},
    {name => 'work_time',             desc => 'Hours Worked', buglist => 1,
     is_numeric => 1},
    {name => 'percentage_complete',   desc => 'Percentage Complete',
     buglist => 1, is_numeric => 1},
    {name => 'content',               desc => 'Content'},
    {name => 'attach_data.thedata',   desc => 'Attachment data'},
    {name => "owner_idle_time",       desc => "Time Since Assignee Touched"},
    {name => 'see_also',              desc => "See Also",
     type => FIELD_TYPE_BUG_URLS},
    {name => 'tag',                   desc => 'Personal Tags', buglist => 1,
     type => FIELD_TYPE_KEYWORDS},
    {name => 'last_visit_ts',         desc => 'Last Visit', buglist => 1,
     type => FIELD_TYPE_DATETIME},
    {name => 'comment_tag',           desc => 'Comment Tag'},
);

################
# Constructors #
################

# Override match to add is_select.
sub match {
    my $self = shift;
    my ($params) = @_;
    if (delete $params->{is_select}) {
        $params->{type} = [FIELD_TYPE_SINGLE_SELECT, FIELD_TYPE_MULTI_SELECT];
    }
    return $self->SUPER::match(@_);
}

##############
# Validators #
##############

sub _check_custom { return $_[1] ? 1 : 0; }

sub _check_description {
    my ($invocant, $desc) = @_;
    $desc = clean_text($desc);
    $desc || ThrowUserError('field_missing_description');
    return $desc;
}

sub _check_long_desc {
    my ($invocant, $long_desc) = @_;
    $long_desc = clean_text($long_desc || '');
    if (length($long_desc) > MAX_FIELD_LONG_DESC_LENGTH) {
        ThrowUserError('field_long_desc_too_long');
    }
    return $long_desc;
}

sub _check_enter_bug { return $_[1] ? 1 : 0; }

sub _check_is_numeric {
    my ($invocant, $value, undef, $params) = @_;
    my $type = blessed($invocant) ? $invocant->type : $params->{type};
    return 1 if $type == FIELD_TYPE_BUG_ID;
    return $value ? 1 : 0;
}

sub _check_mailhead { return $_[1] ? 1 : 0; }

sub _check_name {
    my ($class, $name, undef, $params) = @_;
    $name = lc(clean_text($name));
    $name || ThrowUserError('field_missing_name');

    # Don't want to allow a name that might mess up SQL.
    my $name_regex = qr/^[\w\.]+$/;
    # Custom fields have more restrictive name requirements than
    # standard fields.
    $name_regex = qr/^[a-zA-Z0-9_]+$/ if $params->{custom};
    # Custom fields can't be named just "cf_", and there is no normal
    # field named just "cf_".
    ($name =~ $name_regex && $name ne "cf_")
         || ThrowUserError('field_invalid_name', { name => $name });

    # If it's custom, prepend cf_ to the custom field name to distinguish 
    # it from standard fields.
    if ($name !~ /^cf_/ && $params->{custom}) {
        $name = 'cf_' . $name;
    }

    # Assure the name is unique. Names can't be changed, so we don't have
    # to worry about what to do on updates.
    my $field = new Bugzilla::Field({ name => $name });
    ThrowUserError('field_already_exists', {'field' => $field }) if $field;

    return $name;
}

sub _check_obsolete { return $_[1] ? 1 : 0; }

sub _check_sortkey {
    my ($invocant, $sortkey) = @_;
    my $skey = $sortkey;
    if (!defined $skey || $skey eq '') {
        ($sortkey) = Bugzilla->dbh->selectrow_array(
            'SELECT MAX(sortkey) + 100 FROM fielddefs') || 100;
    }
    detaint_natural($sortkey)
        || ThrowUserError('field_invalid_sortkey', { sortkey => $skey });
    return $sortkey;
}

sub _check_type {
    my ($invocant, $type, undef, $params) = @_;
    my $saved_type = $type;
    (detaint_natural($type) && $type < FIELD_TYPE_HIGHEST_PLUS_ONE)
      || ThrowCodeError('invalid_customfield_type', { type => $saved_type });

    my $custom = blessed($invocant) ? $invocant->custom : $params->{custom};
    if ($custom && !$type) {
        ThrowCodeError('field_type_not_specified');
    }

    return $type;
}

sub _check_value_field_id {
    my ($invocant, $field_id, undef, $params) = @_;
    my $is_select = $invocant->is_select($params);
    if ($field_id && !$is_select) {
        ThrowUserError('field_value_control_select_only');
    }
    return $invocant->_check_visibility_field_id($field_id);
}

sub _check_visibility_field_id {
    my ($invocant, $field_id) = @_;
    $field_id = trim($field_id);
    return undef if !$field_id;
    my $field = Bugzilla::Field->check({ id => $field_id });
    if (blessed($invocant) && $field->id == $invocant->id) {
        ThrowUserError('field_cant_control_self', { field => $field });
    }
    if (!$field->is_select) {
        ThrowUserError('field_control_must_be_select',
                       { field => $field });
    }
    return $field->id;
}

sub _check_visibility_values {
    my ($invocant, $values, undef, $params) = @_;
    my $field;
    if (blessed $invocant) {
        $field = $invocant->visibility_field;
    }
    elsif ($params->{visibility_field_id}) {
        $field = $invocant->new($params->{visibility_field_id});
    }
    # When no field is set, no values are set.
    return [] if !$field;

    if (!scalar @$values) {
        ThrowUserError('field_visibility_values_must_be_selected',
                       { field => $field });
    }

    my @visibility_values;
    my $choice = Bugzilla::Field::Choice->type($field);
    foreach my $value (@$values) {
        if (!blessed $value) {
            $value = $choice->check({ id => $value });
        }
        push(@visibility_values, $value);
    }

    return \@visibility_values;
}

sub _check_reverse_desc {
    my ($invocant, $reverse_desc, undef, $params) = @_;
    my $type = blessed($invocant) ? $invocant->type : $params->{type};
    if ($type != FIELD_TYPE_BUG_ID) {
        return undef; # store NULL for non-reversible field types
    }
    
    $reverse_desc = clean_text($reverse_desc);
    return $reverse_desc;
}

sub _check_is_mandatory { return $_[1] ? 1 : 0; }

=pod

=head2 Instance Properties

=over

=item C<name>

the name of the field in the database; begins with "cf_" if field
is a custom field, but test the value of the boolean "custom" property
to determine if a given field is a custom field;

=item C<description>

a short string describing the field; displayed to Bugzilla users
in several places within Bugzilla's UI, f.e. as the form field label
on the "show bug" page;

=back

=cut

sub description { return $_[0]->{description} }

=over

=item C<long_desc>

A string providing detailed info about the field;

=back

=cut

sub long_desc { return $_[0]->{long_desc} }

=over

=item C<type>

an integer specifying the kind of field this is; values correspond to
the FIELD_TYPE_* constants in Constants.pm

=back

=cut

sub type { return $_[0]->{type} }

=over

=item C<custom>

a boolean specifying whether or not the field is a custom field;
if true, field name should start "cf_", but use this property to determine
which fields are custom fields;

=back

=cut

sub custom { return $_[0]->{custom} }

=over

=item C<in_new_bugmail>

a boolean specifying whether or not the field is displayed in bugmail
for newly-created bugs;

=back

=cut

sub in_new_bugmail { return $_[0]->{mailhead} }

=over

=item C<sortkey>

an integer specifying the sortkey of the field.

=back

=cut

sub sortkey { return $_[0]->{sortkey} }

=over

=item C<obsolete>

a boolean specifying whether or not the field is obsolete;

=back

=cut

sub obsolete { return $_[0]->{obsolete} }

=over

=item C<enter_bug>

A boolean specifying whether or not this field should appear on 
enter_bug.cgi

=back

=cut

sub enter_bug { return $_[0]->{enter_bug} }

=over

=item C<buglist>

A boolean specifying whether or not this field is selectable
as a display or order column in buglist.cgi

=back

=cut

sub buglist { return $_[0]->{buglist} }

=over

=item C<is_select>

True if this is a C<FIELD_TYPE_SINGLE_SELECT> or C<FIELD_TYPE_MULTI_SELECT>
field. It is only safe to call L</legal_values> if this is true.

=item C<legal_values>

Valid values for this field, as an array of L<Bugzilla::Field::Choice>
objects.

=back

=cut

sub is_select {
    my ($invocant, $params) = @_;
    # This allows this method to be called by create() validators.
    my $type = blessed($invocant) ? $invocant->type : $params->{type}; 
    return ($type == FIELD_TYPE_SINGLE_SELECT 
            || $type == FIELD_TYPE_MULTI_SELECT) ? 1 : 0 
}

=over

=item C<is_abnormal>

Most fields that have a C<SELECT> L</type> have a certain schema for
the table that stores their values, the table has the same name as the field,
and the field's legal values can be edited via F<editvalues.cgi>.

However, some fields do not follow that pattern. Those fields are
considered "abnormal".

This method returns C<1> if the field is "abnormal", C<0> otherwise.

=back

=cut

sub is_abnormal {
    my $self = shift;
    return ABNORMAL_SELECTS->{$self->name} ? 1 : 0;
}

sub legal_values {
    my $self = shift;

    if (!defined $self->{'legal_values'}) {
        require Bugzilla::Field::Choice;
        my @values = Bugzilla::Field::Choice->type($self)->get_all();
        $self->{'legal_values'} = \@values;
    }
    return $self->{'legal_values'};
}

=pod

=over

=item C<is_timetracking>

True if this is a time-tracking field that should only be shown to users
in the C<timetrackinggroup>.

=back

=cut

sub is_timetracking {
    my ($self) = @_;
    return grep($_ eq $self->name, TIMETRACKING_FIELDS) ? 1 : 0;
}

=pod

=over

=item C<visibility_field>

What field controls this field's visibility? Returns a C<Bugzilla::Field>
object representing the field that controls this field's visibility.

Returns undef if there is no field that controls this field's visibility.

=back

=cut

sub visibility_field {
    my $self = shift;
    if ($self->{visibility_field_id}) {
        $self->{visibility_field} ||= 
            $self->new($self->{visibility_field_id});
    }
    return $self->{visibility_field};
}

=pod

=over

=item C<visibility_values>

If we have a L</visibility_field>, then what values does that field have to
be set to in order to show this field? Returns a L<Bugzilla::Field::Choice>
or undef if there is no C<visibility_field> set.

=back

=cut

sub visibility_values {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    return [] if !$self->{visibility_field_id};

    if (!defined $self->{visibility_values}) {
        my $visibility_value_ids =
            $dbh->selectcol_arrayref("SELECT value_id FROM field_visibility
                                      WHERE field_id = ?", undef, $self->id);

        $self->{visibility_values} =
            Bugzilla::Field::Choice->type($self->visibility_field)
            ->new_from_list($visibility_value_ids);
    }

    return $self->{visibility_values};
}

=pod

=over

=item C<controls_visibility_of>

An arrayref of C<Bugzilla::Field> objects, representing fields that this
field controls the visibility of.

=back

=cut

sub controls_visibility_of {
    my $self = shift;
    $self->{controls_visibility_of} ||= 
        Bugzilla::Field->match({ visibility_field_id => $self->id });
    return $self->{controls_visibility_of};
}

=pod

=over

=item C<value_field>

The Bugzilla::Field that controls the list of values for this field.

Returns undef if there is no field that controls this field's visibility.

=back

=cut

sub value_field {
    my $self = shift;
    if ($self->{value_field_id}) {
        $self->{value_field} ||= $self->new($self->{value_field_id});
    }
    return $self->{value_field};
}

=pod

=over

=item C<controls_values_of>

An arrayref of C<Bugzilla::Field> objects, representing fields that this
field controls the values of.

=back

=cut

sub controls_values_of {
    my $self = shift;
    $self->{controls_values_of} ||=
        Bugzilla::Field->match({ value_field_id => $self->id });
    return $self->{controls_values_of};
}

=over

=item C<is_visible_on_bug>

See L<Bugzilla::Field::ChoiceInterface>.

=back

=cut

sub is_visible_on_bug {
    my ($self, $bug) = @_;

    # Always return visible, if this field is not
    # visibility controlled.
    return 1 if !$self->{visibility_field_id};

    my $visibility_values = $self->visibility_values;

    return (any { $_->is_set_on_bug($bug) } @$visibility_values) ? 1 : 0;
}

=over

=item C<is_relationship>

Applies only to fields of type FIELD_TYPE_BUG_ID.
Checks to see if a reverse relationship description has been set.
This is the canonical condition to enable reverse link display,
dependency tree display, and similar functionality.

=back

=cut

sub is_relationship  {     
    my $self = shift;
    my $desc = $self->reverse_desc;
    if (defined $desc && $desc ne "") {
        return 1;
    }
    return 0;
}

=over

=item C<reverse_desc>

Applies only to fields of type FIELD_TYPE_BUG_ID.
Describes the reverse relationship of this field.
For example, if a BUG_ID field is called "Is a duplicate of",
the reverse description would be "Duplicates of this bug".

=back

=cut

sub reverse_desc { return $_[0]->{reverse_desc} }

=over

=item C<is_mandatory>

a boolean specifying whether or not the field is mandatory;

=back

=cut

sub is_mandatory { return $_[0]->{is_mandatory} }

=over

=item C<is_numeric>

A boolean specifying whether or not this field logically contains
numeric (integer, decimal, or boolean) values. By "logically contains" we
mean that the user inputs numbers into the value of the field in the UI.
This is mostly used by L<Bugzilla::Search>.

=back

=cut

sub is_numeric { return $_[0]->{is_numeric} }


=pod

=head2 Instance Mutators

These set the particular field that they are named after.

They take a single value--the new value for that field.

They will throw an error if you try to set the values to something invalid.

=over

=item C<set_description>

=item C<set_long_desc>

=item C<set_enter_bug>

=item C<set_obsolete>

=item C<set_sortkey>

=item C<set_in_new_bugmail>

=item C<set_buglist>

=item C<set_reverse_desc>

=item C<set_visibility_field>

=item C<set_visibility_values>

=item C<set_value_field>

=item C<set_is_mandatory>


=back

=cut

sub set_description    { $_[0]->set('description', $_[1]); }
sub set_long_desc      { $_[0]->set('long_desc',   $_[1]); }
sub set_enter_bug      { $_[0]->set('enter_bug',   $_[1]); }
sub set_is_numeric     { $_[0]->set('is_numeric',  $_[1]); }
sub set_obsolete       { $_[0]->set('obsolete',    $_[1]); }
sub set_sortkey        { $_[0]->set('sortkey',     $_[1]); }
sub set_in_new_bugmail { $_[0]->set('mailhead',    $_[1]); }
sub set_buglist        { $_[0]->set('buglist',     $_[1]); }
sub set_reverse_desc    { $_[0]->set('reverse_desc', $_[1]); }
sub set_visibility_field {
    my ($self, $value) = @_;
    $self->set('visibility_field_id', $value);
    delete $self->{visibility_field};
    delete $self->{visibility_values};
}
sub set_visibility_values {
    my ($self, $value_ids) = @_;
    $self->set('visibility_values', $value_ids);
}
sub set_value_field {
    my ($self, $value) = @_;
    $self->set('value_field_id', $value);
    delete $self->{value_field};
}
sub set_is_mandatory { $_[0]->set('is_mandatory', $_[1]); }

# This is only used internally by upgrade code in Bugzilla::Field.
sub _set_type { $_[0]->set('type', $_[1]); }

=pod

=head2 Instance Method

=over

=item C<remove_from_db>

Attempts to remove the passed in field from the database.
Deleting a field is only successful if the field is obsolete and
there are no values specified (or EVER specified) for the field.

=back

=cut

sub remove_from_db {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my $name = $self->name;

    if (!$self->custom) {
        ThrowCodeError('field_not_custom', {'name' => $name });
    }

    if (!$self->obsolete) {
        ThrowUserError('customfield_not_obsolete', {'name' => $self->name });
    }

    $dbh->bz_start_transaction();

    # Check to see if bug activity table has records (should be fast with index)
    my $has_activity = $dbh->selectrow_array("SELECT COUNT(*) FROM bugs_activity
                                      WHERE fieldid = ?", undef, $self->id);
    if ($has_activity) {
        ThrowUserError('customfield_has_activity', {'name' => $name });
    }

    # Check to see if bugs table has records (slow)
    my $bugs_query = "";

    if ($self->type == FIELD_TYPE_MULTI_SELECT) {
        $bugs_query = "SELECT COUNT(*) FROM bug_$name";
    }
    else {
        $bugs_query = "SELECT COUNT(*) FROM bugs WHERE $name IS NOT NULL";
        if ($self->type != FIELD_TYPE_BUG_ID
            && $self->type != FIELD_TYPE_DATE
            && $self->type != FIELD_TYPE_DATETIME)
        {
            $bugs_query .= " AND $name != ''";
        }
        # Ignore the default single select value
        if ($self->type == FIELD_TYPE_SINGLE_SELECT) {
            $bugs_query .= " AND $name != '---'";
        }
    }

    my $has_bugs = $dbh->selectrow_array($bugs_query);
    if ($has_bugs) {
        ThrowUserError('customfield_has_contents', {'name' => $name });
    }

    # Once we reach here, we should be OK to delete.
    $self->SUPER::remove_from_db();

    my $type = $self->type;

    # the values for multi-select are stored in a seperate table
    if ($type != FIELD_TYPE_MULTI_SELECT) {
        $dbh->bz_drop_column('bugs', $name);
    }

    if ($self->is_select) {
        # Delete the table that holds the legal values for this field.
        $dbh->bz_drop_field_tables($self);
    }

    $dbh->bz_commit_transaction()
}

=pod

=head2 Class Methods

=over

=item C<create>

Just like L<Bugzilla::Object/create>. Takes the following parameters:

=over

=item C<name> B<Required> - The name of the field.

=item C<description> B<Required> - The field label to display in the UI.

=item C<long_desc> - A longer description of the field.

=item C<mailhead> - boolean - Whether this field appears at the
top of the bugmail for a newly-filed bug. Defaults to 0.

=item C<custom> - boolean - True if this is a Custom Field. The field
will be added to the C<bugs> table if it does not exist. Defaults to 0.

=item C<sortkey> - integer - The sortkey of the field. Defaults to 0.

=item C<enter_bug> - boolean - Whether this field is
editable on the bug creation form. Defaults to 0.

=item C<buglist> - boolean - Whether this field is
selectable as a display or order column in bug lists. Defaults to 0.

C<obsolete> - boolean - Whether this field is obsolete. Defaults to 0.

C<is_mandatory> - boolean - Whether this field is mandatory. Defaults to 0.

=back

=back

=cut

sub create {
    my $class = shift;
    my ($params) = @_;
    my $dbh = Bugzilla->dbh;

    # This makes sure the "sortkey" validator runs, even if
    # the parameter isn't sent to create().
    $params->{sortkey} = undef if !exists $params->{sortkey};
    $params->{type} ||= 0;
    # We mark the custom field as obsolete till it has been fully created,
    # to avoid race conditions when viewing bugs at the same time.
    my $is_obsolete = $params->{obsolete};
    $params->{obsolete} = 1 if $params->{custom};

    $dbh->bz_start_transaction();
    $class->check_required_create_fields(@_);
    my $field_values      = $class->run_create_validators($params);
    my $visibility_values = delete $field_values->{visibility_values};
    my $field             = $class->insert_create_data($field_values);
    
    $field->set_visibility_values($visibility_values);
    $field->_update_visibility_values();

    $dbh->bz_commit_transaction();
    Bugzilla->memcached->clear_config();

    if ($field->custom) {
        my $name = $field->name;
        my $type = $field->type;
        if (SQL_DEFINITIONS->{$type}) {
            # Create the database column that stores the data for this field.
            $dbh->bz_add_column('bugs', $name, SQL_DEFINITIONS->{$type});
        }

        if ($field->is_select) {
            # Create the table that holds the legal values for this field.
            $dbh->bz_add_field_tables($field);
        }

        if ($type == FIELD_TYPE_SINGLE_SELECT) {
            # Insert a default value of "---" into the legal values table.
            $dbh->do("INSERT INTO $name (value) VALUES ('---')");
        }

        # Restore the original obsolete state of the custom field.
        $dbh->do('UPDATE fielddefs SET obsolete = 0 WHERE id = ?', undef, $field->id)
          unless $is_obsolete;

        Bugzilla->memcached->clear({ table => 'fielddefs', id => $field->id });
        Bugzilla->memcached->clear_config();
    }

    return $field;
}

sub update {
    my $self = shift;
    my $changes = $self->SUPER::update(@_);
    my $dbh = Bugzilla->dbh;
    if ($changes->{value_field_id} && $self->is_select) {
        $dbh->do("UPDATE " . $self->name . " SET visibility_value_id = NULL");
    }
    $self->_update_visibility_values();
    Bugzilla->memcached->clear_config();
    return $changes;
}

sub _update_visibility_values {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my @visibility_value_ids = map($_->id, @{$self->visibility_values});
    $self->_delete_visibility_values();
    for my $value_id (@visibility_value_ids) {
        $dbh->do("INSERT INTO field_visibility (field_id, value_id)
                  VALUES (?, ?)", undef, $self->id, $value_id);
    }
}

sub _delete_visibility_values {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM field_visibility WHERE field_id = ?",
        undef, $self->id);
    delete $self->{visibility_values};
}

=pod

=over

=item C<get_legal_field_values($field)>

Description: returns all the legal values for a field that has a
             list of legal values, like rep_platform or resolution.
             The table where these values are stored must at least have
             the following columns: value, isactive, sortkey.

Params:    C<$field> - Name of the table where valid values are.

Returns:   a reference to a list of valid values.

=back

=cut

sub get_legal_field_values {
    my ($field) = @_;
    my $dbh = Bugzilla->dbh;
    my $result_ref = $dbh->selectcol_arrayref(
         "SELECT value FROM $field
           WHERE isactive = ?
        ORDER BY sortkey, value", undef, (1));
    return $result_ref;
}

=over

=item C<populate_field_definitions()>

Description: Populates the fielddefs table during an installation
             or upgrade.

Params:      none

Returns:     nothing

=back

=cut

sub populate_field_definitions {
    my $dbh = Bugzilla->dbh;

    # ADD and UPDATE field definitions
    foreach my $def (DEFAULT_FIELDS) {
        my $field = new Bugzilla::Field({ name => $def->{name} });
        if ($field) {
            $field->set_description($def->{desc});
            $field->set_in_new_bugmail($def->{in_new_bugmail});
            $field->set_buglist($def->{buglist});
            $field->_set_type($def->{type}) if $def->{type};
            $field->set_is_mandatory($def->{is_mandatory});
            $field->set_is_numeric($def->{is_numeric});
            $field->update();
        }
        else {
            if (exists $def->{in_new_bugmail}) {
                $def->{mailhead} = $def->{in_new_bugmail};
                delete $def->{in_new_bugmail};
            }
            $def->{description} = delete $def->{desc};
            Bugzilla::Field->create($def);
        }
    }

    # DELETE fields which were added only accidentally, or which
    # were never tracked in bugs_activity. Note that you can never
    # delete fields which are used by bugs_activity.

    # Oops. Bug 163299
    $dbh->do("DELETE FROM fielddefs WHERE name='cc_accessible'");
    # Oops. Bug 215319
    $dbh->do("DELETE FROM fielddefs WHERE name='requesters.login_name'");
    # This field was never tracked in bugs_activity, so it's safe to delete.
    $dbh->do("DELETE FROM fielddefs WHERE name='attachments.thedata'");

    # MODIFY old field definitions

    # 2005-11-13 LpSolit@gmail.com - Bug 302599
    # One of the field names was a fragment of SQL code, which is DB dependent.
    # We have to rename it to a real name, which is DB independent.
    my $new_field_name = 'days_elapsed';
    my $field_description = 'Days since bug changed';

    my ($old_field_id, $old_field_name) =
        $dbh->selectrow_array('SELECT id, name FROM fielddefs
                                WHERE description = ?',
                              undef, $field_description);

    if ($old_field_id && ($old_field_name ne $new_field_name)) {
        say "SQL fragment found in the 'fielddefs' table...";
        say "Old field name: $old_field_name";
        # We have to fix saved searches first. Queries have been escaped
        # before being saved. We have to do the same here to find them.
        $old_field_name = url_quote($old_field_name);
        my $broken_named_queries =
            $dbh->selectall_arrayref('SELECT userid, name, query
                                        FROM namedqueries WHERE ' .
                                      $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                      undef, "%=$old_field_name%");

        my $sth_UpdateQueries = $dbh->prepare('UPDATE namedqueries SET query = ?
                                                WHERE userid = ? AND name = ?');

        print "Fixing saved searches...\n" if scalar(@$broken_named_queries);
        foreach my $named_query (@$broken_named_queries) {
            my ($userid, $name, $query) = @$named_query;
            $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
            $sth_UpdateQueries->execute($query, $userid, $name);
        }

        # We now do the same with saved chart series.
        my $broken_series =
            $dbh->selectall_arrayref('SELECT series_id, query
                                        FROM series WHERE ' .
                                      $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                      undef, "%=$old_field_name%");

        my $sth_UpdateSeries = $dbh->prepare('UPDATE series SET query = ?
                                               WHERE series_id = ?');

        print "Fixing saved chart series...\n" if scalar(@$broken_series);
        foreach my $series (@$broken_series) {
            my ($series_id, $query) = @$series;
            $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
            $sth_UpdateSeries->execute($query, $series_id);
        }
        # Now that saved searches have been fixed, we can fix the field name.
        say "Fixing the 'fielddefs' table...";
        say "New field name: $new_field_name";
        $dbh->do('UPDATE fielddefs SET name = ? WHERE id = ?',
                  undef, ($new_field_name, $old_field_id));
    }

    # This field has to be created separately, or the above upgrade code
    # might not run properly.
    Bugzilla::Field->create({ name => $new_field_name, 
                              description => $field_description })
        unless new Bugzilla::Field({ name => $new_field_name });

}



=head2 Data Validation

=over

=item C<check_field($name, $value, \@legal_values, $no_warn)>

Description: Makes sure the field $name is defined and its $value
             is non empty. If @legal_values is defined, this routine
             checks whether its value is one of the legal values
             associated with this field, else it checks against
             the default valid values for this field obtained by
             C<get_legal_field_values($name)>. If the test is successful,
             the function returns 1. If the test fails, an error
             is thrown (by default), unless $no_warn is true, in which
             case the function returns 0.

Params:      $name         - the field name
             $value        - the field value
             @legal_values - (optional) list of legal values
             $no_warn      - (optional) do not throw an error if true

Returns:     1 on success; 0 on failure if $no_warn is true (else an
             error is thrown).

=back

=cut

sub check_field {
    my ($name, $value, $legalsRef, $no_warn) = @_;
    my $dbh = Bugzilla->dbh;

    # If $legalsRef is undefined, we use the default valid values.
    # Valid values for this check are all possible values. 
    # Using get_legal_values would only return active values, but since
    # some bugs may have inactive values set, we want to check them too. 
    unless (defined $legalsRef) {
        $legalsRef = Bugzilla::Field->new({name => $name})->legal_values;
        my @values = map($_->name, @$legalsRef);
        $legalsRef = \@values;

    }

    if (!defined($value)
        or trim($value) eq ""
        or !grep { $_ eq $value } @$legalsRef)
    {
        return 0 if $no_warn; # We don't want an error to be thrown; return.
        trick_taint($name);

        my $field = new Bugzilla::Field({ name => $name });
        my $field_desc = $field ? $field->description : $name;
        ThrowCodeError('illegal_field', { field => $field_desc });
    }
    return 1;
}

=pod

=over

=item C<get_field_id($fieldname)>

Description: Returns the ID of the specified field name and throws
             an error if this field does not exist.

Params:      $fieldname - a field name

Returns:     the corresponding field ID or an error if the field name
             does not exist.

=back

=cut

sub get_field_id {
    my $field = Bugzilla->fields({ by_name => 1 })->{$_[0]}
      or ThrowCodeError('invalid_field_name', {field => $_[0]});

    return $field->id;
}

1;

__END__

=head1 B<Methods in need of POD>

=over

=item match

=item set_is_numeric

=item update

=back
