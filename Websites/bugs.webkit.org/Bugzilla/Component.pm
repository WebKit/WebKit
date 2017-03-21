# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Component;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Field::ChoiceInterface Bugzilla::Object);

use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::FlagType;
use Bugzilla::Series;

use Scalar::Util qw(blessed);

###############################
####    Initialization     ####
###############################

use constant DB_TABLE => 'components';
# This is mostly for the editfields.cgi case where ->get_all is called.
use constant LIST_ORDER => 'product_id, name';

use constant DB_COLUMNS => qw(
    id
    name
    product_id
    initialowner
    initialqacontact
    description
    isactive
);

use constant UPDATE_COLUMNS => qw(
    name
    initialowner
    initialqacontact
    description
    isactive
);

use constant REQUIRED_FIELD_MAP => {
    product_id => 'product',
};

use constant VALIDATORS => {
    create_series    => \&Bugzilla::Object::check_boolean,
    product          => \&_check_product,
    initialowner     => \&_check_initialowner,
    initialqacontact => \&_check_initialqacontact,
    description      => \&_check_description,
    initial_cc       => \&_check_cc_list,
    name             => \&_check_name,
    isactive         => \&Bugzilla::Object::check_boolean,
};

use constant VALIDATOR_DEPENDENCIES => {
    name => ['product'],
};

###############################

sub new {
    my $class = shift;
    my $param = shift;
    my $dbh = Bugzilla->dbh;

    my $product;
    if (ref $param and !defined $param->{id}) {
        $product = $param->{product};
        my $name = $param->{name};
        if (!defined $product) {
            ThrowCodeError('bad_arg',
                {argument => 'product',
                 function => "${class}::new"});
        }
        if (!defined $name) {
            ThrowCodeError('bad_arg',
                {argument => 'name',
                 function => "${class}::new"});
        }

        my $condition = 'product_id = ? AND name = ?';
        my @values = ($product->id, $name);
        $param = { condition => $condition, values => \@values };
    }

    unshift @_, $param;
    my $component = $class->SUPER::new(@_);
    # Add the product object as attribute only if the component exists.
    $component->{product} = $product if ($component && $product);
    return $component;
}

sub create {
    my $class = shift;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();

    $class->check_required_create_fields(@_);
    my $params = $class->run_create_validators(@_);
    my $cc_list = delete $params->{initial_cc};
    my $create_series = delete $params->{create_series};
    my $product = delete $params->{product};
    $params->{product_id} = $product->id;

    my $component = $class->insert_create_data($params);
    $component->{product} = $product;

    # We still have to fill the component_cc table.
    $component->_update_cc_list($cc_list) if $cc_list;

    # Create series for the new component.
    $component->_create_series() if $create_series;

    $dbh->bz_commit_transaction();
    return $component;
}

sub update {
    my $self = shift;
    my $changes = $self->SUPER::update(@_);

    # Update the component_cc table if necessary.
    if (defined $self->{cc_ids}) {
        my $diff = $self->_update_cc_list($self->{cc_ids});
        $changes->{cc_list} = $diff if defined $diff;
    }
    return $changes;
}

sub remove_from_db {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    $self->_check_if_controller(); # From ChoiceInterface

    $dbh->bz_start_transaction();

    # Products must have at least one component.
    my @components = @{ $self->product->components };
    if (scalar(@components) == 1) {
        ThrowUserError('component_is_last', { comp => $self });
    }

    if ($self->bug_count) {
        if (Bugzilla->params->{'allowbugdeletion'}) {
            require Bugzilla::Bug;
            foreach my $bug_id (@{$self->bug_ids}) {
                # Note: We allow admins to delete bugs even if they can't
                # see them, as long as they can see the product.
                my $bug = new Bugzilla::Bug($bug_id);
                $bug->remove_from_db();
            }
        } else {
            ThrowUserError('component_has_bugs', {nb => $self->bug_count});
        }
    }
    # Update the list of components in the product object.
    $self->product->{components} = [grep { $_->id != $self->id } @components];
    $self->SUPER::remove_from_db();

    $dbh->bz_commit_transaction();
}

################################
# Validators
################################

sub _check_name {
    my ($invocant, $name, undef, $params) = @_;
    my $product = blessed($invocant) ? $invocant->product : $params->{product};

    $name = trim($name);
    $name || ThrowUserError('component_blank_name');

    if (length($name) > MAX_COMPONENT_SIZE) {
        ThrowUserError('component_name_too_long', {'name' => $name});
    }

    my $component = new Bugzilla::Component({product => $product, name => $name});
    if ($component && (!ref $invocant || $component->id != $invocant->id)) {
        ThrowUserError('component_already_exists', { name    => $component->name,
                                                     product => $product });
    }
    return $name;
}

sub _check_description {
    my ($invocant, $description) = @_;

    $description = trim($description);
    $description || ThrowUserError('component_blank_description');
    return $description;
}

sub _check_initialowner {
    my ($invocant, $owner) = @_;

    $owner || ThrowUserError('component_need_initialowner');
    my $owner_id = Bugzilla::User->check($owner)->id;
    return $owner_id;
}

sub _check_initialqacontact {
    my ($invocant, $qa_contact) = @_;

    my $qa_contact_id;
    if (Bugzilla->params->{'useqacontact'}) {
        $qa_contact_id = Bugzilla::User->check($qa_contact)->id if $qa_contact;
    }
    elsif (ref $invocant) {
        $qa_contact_id = $invocant->{initialqacontact};
    }
    return $qa_contact_id;
}

sub _check_product {
    my ($invocant, $product) = @_;
    $product || ThrowCodeError('param_required', 
                    { function => "$invocant->create", param => 'product' });
    return Bugzilla->user->check_can_admin_product($product->name);
}

sub _check_cc_list {
    my ($invocant, $cc_list) = @_;

    my %cc_ids;
    foreach my $cc (@$cc_list) {
        my $id = login_to_id($cc, THROW_ERROR);
        $cc_ids{$id} = 1;
    }
    return [keys %cc_ids];
}

###############################
####       Methods         ####
###############################

sub _update_cc_list {
    my ($self, $cc_list) = @_;
    my $dbh = Bugzilla->dbh;

    my $old_cc_list =
      $dbh->selectcol_arrayref('SELECT user_id FROM component_cc
                                WHERE component_id = ?', undef, $self->id);

    my ($removed, $added) = diff_arrays($old_cc_list, $cc_list);
    my $diff;
    if (scalar @$removed || scalar @$added) {
        $diff = [join(', ', @$removed), join(', ', @$added)];
    }

    $dbh->do('DELETE FROM component_cc WHERE component_id = ?', undef, $self->id);

    my $sth = $dbh->prepare('INSERT INTO component_cc
                             (user_id, component_id) VALUES (?, ?)');
    $sth->execute($_, $self->id) foreach (@$cc_list);

    return $diff;
}

sub _create_series {
    my $self = shift;

    # Insert default charting queries for this product.
    # If they aren't using charting, this won't do any harm.
    my $prodcomp = "&product="   . url_quote($self->product->name) .
                   "&component=" . url_quote($self->name);

    my $open_query = 'field0-0-0=resolution&type0-0-0=notregexp&value0-0-0=.' .
                     $prodcomp;
    my $nonopen_query = 'field0-0-0=resolution&type0-0-0=regexp&value0-0-0=.' .
                        $prodcomp;

    my @series = ([get_text('series_all_open'), $open_query],
                  [get_text('series_all_closed'), $nonopen_query]);

    foreach my $sdata (@series) {
        my $series = new Bugzilla::Series(undef, $self->product->name,
                                          $self->name, $sdata->[0],
                                          Bugzilla->user->id, 1, $sdata->[1], 1);
        $series->writeToDatabase();
    }
}

sub set_name { $_[0]->set('name', $_[1]); }
sub set_description { $_[0]->set('description', $_[1]); }
sub set_is_active { $_[0]->set('isactive', $_[1]); }
sub set_default_assignee {
    my ($self, $owner) = @_;

    $self->set('initialowner', $owner);
    # Reset the default owner object.
    delete $self->{default_assignee};
}
sub set_default_qa_contact {
    my ($self, $qa_contact) = @_;

    $self->set('initialqacontact', $qa_contact);
    # Reset the default QA contact object.
    delete $self->{default_qa_contact};
}
sub set_cc_list {
    my ($self, $cc_list) = @_;

    $self->{cc_ids} = $self->_check_cc_list($cc_list);
    # Reset the list of CC user objects.
    delete $self->{initial_cc};
}

sub bug_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bug_count'}) {
        $self->{'bug_count'} = $dbh->selectrow_array(q{
            SELECT COUNT(*) FROM bugs
            WHERE component_id = ?}, undef, $self->id) || 0;
    }
    return $self->{'bug_count'};
}

sub bug_ids {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bugs_ids'}) {
        $self->{'bugs_ids'} = $dbh->selectcol_arrayref(q{
            SELECT bug_id FROM bugs
            WHERE component_id = ?}, undef, $self->id);
    }
    return $self->{'bugs_ids'};
}

sub default_assignee {
    my $self = shift;

    return $self->{'default_assignee'}
      ||= new Bugzilla::User({ id => $self->{'initialowner'}, cache => 1 });
}

sub default_qa_contact {
    my $self = shift;

    return unless $self->{'initialqacontact'};
    return $self->{'default_qa_contact'}
      ||= new Bugzilla::User({id => $self->{'initialqacontact'}, cache => 1 });
}

sub flag_types {
    my $self = shift;

    if (!defined $self->{'flag_types'}) {
        my $flagtypes = Bugzilla::FlagType::match({ product_id   => $self->product_id,
                                                    component_id => $self->id });

        $self->{'flag_types'} = {};
        $self->{'flag_types'}->{'bug'} =
          [grep { $_->target_type eq 'bug' } @$flagtypes];
        $self->{'flag_types'}->{'attachment'} =
          [grep { $_->target_type eq 'attachment' } @$flagtypes];
    }
    return $self->{'flag_types'};
}

sub initial_cc {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'initial_cc'}) {
        # If set_cc_list() has been called but data are not yet written
        # into the DB, we want the new values defined by it.
        my $cc_ids = $self->{cc_ids}
                     || $dbh->selectcol_arrayref('SELECT user_id FROM component_cc
                                                  WHERE component_id = ?',
                                                  undef, $self->id);

        $self->{'initial_cc'} = Bugzilla::User->new_from_list($cc_ids);
    }
    return $self->{'initial_cc'};
}

sub product {
    my $self = shift;
    if (!defined $self->{'product'}) {
        require Bugzilla::Product; # We cannot |use| it.
        $self->{'product'} = new Bugzilla::Product($self->product_id);
    }
    return $self->{'product'};
}

###############################
####      Accessors        ####
###############################

sub description { return $_[0]->{'description'}; }
sub product_id  { return $_[0]->{'product_id'};  }
sub is_active   { return $_[0]->{'isactive'};    }

##############################################
# Implement Bugzilla::Field::ChoiceInterface #
##############################################

use constant FIELD_NAME => 'component';
use constant is_default => 0;

sub is_set_on_bug {
    my ($self, $bug) = @_;
    my $value = blessed($bug) ? $bug->component_id : $bug->{component};
    $value = $value->id if blessed($value);
    return 0 unless $value;
    return $value == $self->id ? 1 : 0;
}

###############################
####      Subroutines      ####
###############################

1;

__END__

=head1 NAME

Bugzilla::Component - Bugzilla product component class.

=head1 SYNOPSIS

    use Bugzilla::Component;

    my $component = new Bugzilla::Component($comp_id);
    my $component = new Bugzilla::Component({ product => $product, name => $name });

    my $bug_count          = $component->bug_count();
    my $bug_ids            = $component->bug_ids();
    my $id                 = $component->id;
    my $name               = $component->name;
    my $description        = $component->description;
    my $product_id         = $component->product_id;
    my $default_assignee   = $component->default_assignee;
    my $default_qa_contact = $component->default_qa_contact;
    my $initial_cc         = $component->initial_cc;
    my $product            = $component->product;
    my $bug_flag_types     = $component->flag_types->{'bug'};
    my $attach_flag_types  = $component->flag_types->{'attachment'};

    my $component = Bugzilla::Component->check({ product => $product, name => $name });

    my $component =
      Bugzilla::Component->create({ name             => $name,
                                    product          => $product,
                                    initialowner     => $user_login1,
                                    initialqacontact => $user_login2,
                                    description      => $description});

    $component->set_name($new_name);
    $component->set_description($new_description);
    $component->set_default_assignee($new_login_name);
    $component->set_default_qa_contact($new_login_name);
    $component->set_cc_list(\@new_login_names);
    $component->update();

    $component->remove_from_db;

=head1 DESCRIPTION

Component.pm represents a Product Component object.

=head1 METHODS

=over

=item C<new($param)>

 Description: The constructor is used to load an existing component
              by passing a component ID or a hash with the product
              object the component belongs to and the component name.

 Params:      $param - If you pass an integer, the integer is the
                       component ID from the database that we want to
                       read in. 
                       However, If you pass in a hash, it must contain
                       two keys:
                       name (string): the name of the component
                       product (object): an object of Bugzilla::Product
                       representing the product that the component belongs to.

 Returns:     A Bugzilla::Component object.

=item C<bug_count()>

 Description: Returns the total of bugs that belong to the component.

 Params:      none.

 Returns:     Integer with the number of bugs.

=item C<bug_ids()>

 Description: Returns all bug IDs that belong to the component.

 Params:      none.

 Returns:     A reference to an array of bug IDs.

=item C<default_assignee()>

 Description: Returns a user object that represents the default assignee for
              the component.

 Params:      none.

 Returns:     A Bugzilla::User object.

=item C<default_qa_contact()>

 Description: Returns a user object that represents the default QA contact for
              the component.

 Params:      none.

 Returns:     A Bugzilla::User object if the default QA contact is defined for
              the component. Otherwise, returns undef.

=item C<initial_cc>

 Description: Returns a list of user objects representing users being
              in the initial CC list.

 Params:      none.

 Returns:     An arrayref of L<Bugzilla::User> objects.

=item C<flag_types()>

 Description: Returns all bug and attachment flagtypes available for
              the component.

 Params:      none.

 Returns:     Two references to an array of flagtype objects.

=item C<product()>

 Description: Returns the product the component belongs to.

 Params:      none.

 Returns:     A Bugzilla::Product object.

=item C<set_name($new_name)>

 Description: Changes the name of the component.

 Params:      $new_name - new name of the component (string). This name
                          must be unique within the product.

 Returns:     Nothing.

=item C<set_description($new_desc)>

 Description: Changes the description of the component.

 Params:      $new_desc - new description of the component (string).

 Returns:     Nothing.

=item C<set_default_assignee($new_assignee)>

 Description: Changes the default assignee of the component.

 Params:      $new_owner - login name of the new default assignee of
                           the component (string). This user account
                           must already exist.

 Returns:     Nothing.

=item C<set_default_qa_contact($new_qa_contact)>

 Description: Changes the default QA contact of the component.

 Params:      $new_qa_contact - login name of the new QA contact of
                                the component (string). This user
                                account must already exist.

 Returns:     Nothing.

=item C<set_cc_list(\@cc_list)>

 Description: Changes the list of users being in the CC list by default.

 Params:      \@cc_list - list of login names (string). All the user
                          accounts must already exist.

 Returns:     Nothing.

=item C<update()>

 Description: Write changes made to the component into the DB.

 Params:      none.

 Returns:     A hashref with changes made to the component object.

=item C<remove_from_db()>

 Description: Deletes the current component from the DB. The object itself
              is not destroyed.

 Params:      none.

 Returns:     Nothing.

=back

=head1 CLASS METHODS

=over

=item C<create(\%params)>

 Description: Create a new component for the given product.

 Params:      The hashref must have the following keys:
              name             - name of the new component (string). This name
                                 must be unique within the product.
              product          - a Bugzilla::Product object to which
                                 the Component is being added.
              description      - description of the new component (string).
              initialowner     - login name of the default assignee (string).
              The following keys are optional:
              initialqacontact - login name of the default QA contact (string),
                                 or an empty string to clear it.
              initial_cc       - an arrayref of login names to add to the
                                 CC list by default.

 Returns:     A Bugzilla::Component object.

=back

=cut

=head1 B<Methods in need of POD>

=over

=item is_set_on_bug

=item product_id

=item set_is_active

=item description

=item is_active

=back
