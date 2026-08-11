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
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::Group;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Product;
use Bugzilla::Token;

# Make sure the user is logged in and has the right privileges.
my $user = Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

print $cgi->header();

$user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "flagtypes"});

# We need this everywhere.
my $vars = get_products_and_components();
my @products = @{$vars->{products}};

my $action = $cgi->param('action') || 'list';
my $token  = $cgi->param('token');
my $prod_name = $cgi->param('product');
my $comp_name = $cgi->param('component');
my $flag_id = $cgi->param('id');

my ($product, $component);

if ($prod_name) {
    # Make sure the user is allowed to view this product name.
    # Users with global editcomponents privs can see all product names.
    ($product) = grep { lc($_->name) eq lc($prod_name) } @products;
    $product || ThrowUserError('product_access_denied', { name => $prod_name });
}

if ($comp_name) {
    $product || ThrowUserError('flag_type_component_without_product');
    ($component) = grep { lc($_->name) eq lc($comp_name) } @{$product->components};
    $component || ThrowUserError('product_unknown_component', { product => $product->name,
                                                                comp => $comp_name });
}

# If 'categoryAction' is set, it has priority over 'action'.
if (my ($category_action) = grep { $_ =~ /^categoryAction-(?:\w+)$/ } $cgi->param()) {
    $category_action =~ s/^categoryAction-//;

    my @inclusions = $cgi->param('inclusions');
    my @exclusions = $cgi->param('exclusions');
    my @categories;
    if ($category_action =~ /^(in|ex)clude$/) {
        if (!$user->in_group('editcomponents') && !$product) {
            # The user can only add the flag type to products they can administrate.
            foreach my $prod (@products) {
                push(@categories, $prod->id . ':0')
            }
        }
        else {
            my $category = ($product ? $product->id : 0) . ':' .
                           ($component ? $component->id : 0);
            push(@categories, $category);
        }
    }

    if ($category_action eq 'include') {
        foreach my $category (@categories) {
            push(@inclusions, $category) unless grep($_ eq $category, @inclusions);
        }
    }
    elsif ($category_action eq 'exclude') {
        foreach my $category (@categories) {
            push(@exclusions, $category) unless grep($_ eq $category, @exclusions);
        }
    }
    elsif ($category_action eq 'removeInclusion') {
        my @inclusion_to_remove = $cgi->param('inclusion_to_remove');
        foreach my $remove (@inclusion_to_remove) {
            @inclusions = grep { $_ ne $remove } @inclusions;
        }
    }
    elsif ($category_action eq 'removeExclusion') {
        my @exclusion_to_remove = $cgi->param('exclusion_to_remove');
        foreach my $remove (@exclusion_to_remove) {
            @exclusions = grep { $_ ne $remove } @exclusions;
        }
    }

    $vars->{'groups'} = get_settable_groups();
    $vars->{'action'} = $action;

    my $type = {};
    $type->{$_} = $cgi->param($_) foreach $cgi->param();
    # Make sure boolean fields are defined, else they fall back to 1.
    foreach my $boolean (qw(is_active is_requestable is_requesteeble is_multiplicable)) {
        $type->{$boolean} ||= 0;
    }

    # That's what I call a big hack. The template expects to see a group object.
    $type->{'grant_group'} = {};
    $type->{'grant_group'}->{'name'} = $cgi->param('grant_group');
    $type->{'request_group'} = {};
    $type->{'request_group'}->{'name'} = $cgi->param('request_group');

    $vars->{'inclusions'} = clusion_array_to_hash(\@inclusions, \@products);
    $vars->{'exclusions'} = clusion_array_to_hash(\@exclusions, \@products);

    $vars->{'type'} = $type;
    $vars->{'token'} = $token;
    $vars->{'check_clusions'} = 1;
    $vars->{'can_fully_edit'} = $cgi->param('can_fully_edit');

    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'list') {
    my $product_id = $product ? $product->id : 0;
    my $component_id = $component ? $component->id : 0;
    my $show_flag_counts = $cgi->param('show_flag_counts') ? 1 : 0;
    my $group_id = $cgi->param('group');
    if ($group_id) {
        detaint_natural($group_id) || ThrowUserError('invalid_group_ID');
    }

    my $bug_flagtypes;
    my $attach_flagtypes;

    # If a component is given, restrict the list to flag types available
    # for this component.
    if ($component) {
        $bug_flagtypes = $component->flag_types->{'bug'};
        $attach_flagtypes = $component->flag_types->{'attachment'};

        # Filter flag types if a group ID is given.
        $bug_flagtypes = filter_group($bug_flagtypes, $group_id);
        $attach_flagtypes = filter_group($attach_flagtypes, $group_id);

    }
    # If only a product is specified but no component, then restrict the list
    # to flag types available in at least one component of that product.
    elsif ($product) {
        $bug_flagtypes = $product->flag_types->{'bug'};
        $attach_flagtypes = $product->flag_types->{'attachment'};

        # Filter flag types if a group ID is given.
        $bug_flagtypes = filter_group($bug_flagtypes, $group_id);
        $attach_flagtypes = filter_group($attach_flagtypes, $group_id);
    }
    # If no product is given, then show all flag types available.
    else {
        my $flagtypes = get_editable_flagtypes(\@products, $group_id);
        $bug_flagtypes = [grep { $_->target_type eq 'bug' } @$flagtypes];
        $attach_flagtypes = [grep { $_->target_type eq 'attachment' } @$flagtypes];
    }

    if ($show_flag_counts) {
        my %bug_lists;
        my %map = ('+' => 'granted', '-' => 'denied', '?' => 'pending');

        foreach my $flagtype (@$bug_flagtypes, @$attach_flagtypes) {
            $bug_lists{$flagtype->id} = {};
            my $flags = Bugzilla::Flag->match({type_id => $flagtype->id});
            # Build lists of bugs, triaged by flag status.
            push(@{$bug_lists{$flagtype->id}->{$map{$_->status}}}, $_->bug_id) foreach @$flags;
        }
        $vars->{'bug_lists'} = \%bug_lists;
        $vars->{'show_flag_counts'} = 1;
    }

    $vars->{'selected_product'} = $product ? $product->name : '';
    $vars->{'selected_component'} = $component ? $component->name : '';
    $vars->{'bug_types'} = $bug_flagtypes;
    $vars->{'attachment_types'} = $attach_flagtypes;

    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'enter') {
    my $type = $cgi->param('target_type');
    ($type eq 'bug' || $type eq 'attachment')
      || ThrowCodeError('flag_type_target_type_invalid', { target_type => $type });

    $vars->{'action'} = 'insert';
    $vars->{'token'} = issue_session_token('add_flagtype');
    $vars->{'type'} = { 'target_type' => $type };
    # Only users with global editcomponents privs can add a flagtype
    # to all products.
    $vars->{'inclusions'} = { '__Any__:__Any__' => '0:0' }
      if $user->in_group('editcomponents');
    $vars->{'can_fully_edit'} = 1;
    # Get a list of groups available to restrict this flag type against.
    $vars->{'groups'} = get_settable_groups();

    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'edit' || $action eq 'copy') {
    my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_id);
    $vars->{'type'} = $flagtype;
    $vars->{'can_fully_edit'} = $can_fully_edit;

    if ($user->in_group('editcomponents')) {
        $vars->{'inclusions'} = $flagtype->inclusions;
        $vars->{'exclusions'} = $flagtype->exclusions;
    }
    else {
        # Filter products the user shouldn't know about.
        $vars->{'inclusions'} = clusion_array_to_hash([values %{$flagtype->inclusions}], \@products);
        $vars->{'exclusions'} = clusion_array_to_hash([values %{$flagtype->exclusions}], \@products);
    }

    if ($action eq 'copy') {
        $vars->{'action'} = "insert";
        $vars->{'token'} = issue_session_token('add_flagtype');
    }
    else { 
        $vars->{'action'} = "update";
        $vars->{'token'} = issue_session_token('edit_flagtype');
    }

    # Get a list of groups available to restrict this flag type against.
    $vars->{'groups'} = get_settable_groups();

    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'insert') {
    check_token_data($token, 'add_flagtype');

    my $name             = $cgi->param('name');
    my $description      = $cgi->param('description');
    my $target_type      = $cgi->param('target_type');
    my $cc_list          = $cgi->param('cc_list');
    my $sortkey          = $cgi->param('sortkey');
    my $is_active        = $cgi->param('is_active');
    my $is_requestable   = $cgi->param('is_requestable');
    my $is_specifically  = $cgi->param('is_requesteeble');
    my $is_multiplicable = $cgi->param('is_multiplicable');
    my $grant_group      = $cgi->param('grant_group');
    my $request_group    = $cgi->param('request_group');
    my @inclusions       = $cgi->param('inclusions');
    my @exclusions       = $cgi->param('exclusions');

    # Filter inclusion and exclusion lists to products the user can see.
    unless ($user->in_group('editcomponents')) {
        @inclusions = values %{clusion_array_to_hash(\@inclusions, \@products)};
        @exclusions = values %{clusion_array_to_hash(\@exclusions, \@products)};
    }

    my $flagtype = Bugzilla::FlagType->create({
        name        => $name,
        description => $description,
        target_type => $target_type,
        cc_list     => $cc_list,
        sortkey     => $sortkey,
        is_active   => $is_active,
        is_requestable   => $is_requestable,
        is_requesteeble  => $is_specifically,
        is_multiplicable => $is_multiplicable,
        grant_group      => $grant_group,
        request_group    => $request_group,
        inclusions       => \@inclusions,
        exclusions       => \@exclusions
    });

    delete_token($token);

    $vars->{'name'} = $flagtype->name;
    $vars->{'message'} = "flag_type_created";

    my $flagtypes = get_editable_flagtypes(\@products);
    $vars->{'bug_types'} = [grep { $_->target_type eq 'bug' } @$flagtypes];
    $vars->{'attachment_types'} = [grep { $_->target_type eq 'attachment' } @$flagtypes];

    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'update') {
    check_token_data($token, 'edit_flagtype');

    my $name             = $cgi->param('name');
    my $description      = $cgi->param('description');
    my $cc_list          = $cgi->param('cc_list');
    my $sortkey          = $cgi->param('sortkey');
    my $is_active        = $cgi->param('is_active');
    my $is_requestable   = $cgi->param('is_requestable');
    my $is_specifically  = $cgi->param('is_requesteeble');
    my $is_multiplicable = $cgi->param('is_multiplicable');
    my $grant_group      = $cgi->param('grant_group');
    my $request_group    = $cgi->param('request_group');
    my @inclusions       = $cgi->param('inclusions');
    my @exclusions       = $cgi->param('exclusions');

    my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_id);
    if ($cgi->param('check_clusions') && !$user->in_group('editcomponents')) {
        # Filter inclusion and exclusion lists to products the user can edit.
        @inclusions = values %{clusion_array_to_hash(\@inclusions, \@products)};
        @exclusions = values %{clusion_array_to_hash(\@exclusions, \@products)};
        # Bring back the products the user cannot edit.
        foreach my $item (values %{$flagtype->inclusions}) {
            my ($prod_id, $comp_id) = split(':', $item);
            push(@inclusions, $item) unless grep { $_->id == $prod_id } @products;
        }
        foreach my $item (values %{$flagtype->exclusions}) {
            my ($prod_id, $comp_id) = split(':', $item);
            push(@exclusions, $item) unless grep { $_->id == $prod_id } @products;
        }
    }

    if ($can_fully_edit) {
        $flagtype->set_name($name);
        $flagtype->set_description($description);
        $flagtype->set_cc_list($cc_list);
        $flagtype->set_sortkey($sortkey);
        $flagtype->set_is_active($is_active);
        $flagtype->set_is_requestable($is_requestable);
        $flagtype->set_is_specifically_requestable($is_specifically);
        $flagtype->set_is_multiplicable($is_multiplicable);
        $flagtype->set_grant_group($grant_group);
        $flagtype->set_request_group($request_group);
    }
    $flagtype->set_clusions({ inclusions => \@inclusions, exclusions => \@exclusions})
      if $cgi->param('check_clusions');
    my $changes = $flagtype->update();

    delete_token($token);

    $vars->{'flagtype'} = $flagtype;
    $vars->{'changes'} = $changes;
    $vars->{'message'} = 'flag_type_updated';

    my $flagtypes = get_editable_flagtypes(\@products);
    $vars->{'bug_types'} = [grep { $_->target_type eq 'bug' } @$flagtypes];
    $vars->{'attachment_types'} = [grep { $_->target_type eq 'attachment' } @$flagtypes];

    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'confirmdelete') {
    my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_id);
    ThrowUserError('flag_type_cannot_delete', { flagtype => $flagtype }) unless $can_fully_edit;

    $vars->{'flag_type'} = $flagtype;
    $vars->{'token'} = issue_session_token('delete_flagtype');

    $template->process("admin/flag-type/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'delete') {
    check_token_data($token, 'delete_flagtype');

    my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_id);
    ThrowUserError('flag_type_cannot_delete', { flagtype => $flagtype }) unless $can_fully_edit;

    $flagtype->remove_from_db();

    delete_token($token);

    $vars->{'name'} = $flagtype->name;
    $vars->{'message'} = "flag_type_deleted";

    my @flagtypes = Bugzilla::FlagType->get_all;
    $vars->{'bug_types'} = [grep { $_->target_type eq 'bug' } @flagtypes];
    $vars->{'attachment_types'} = [grep { $_->target_type eq 'attachment' } @flagtypes];

    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'deactivate') {
    check_token_data($token, 'delete_flagtype');

    my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_id);
    ThrowUserError('flag_type_cannot_deactivate', { flagtype => $flagtype }) unless $can_fully_edit;

    $flagtype->set_is_active(0);
    $flagtype->update();

    delete_token($token);

    $vars->{'message'} = "flag_type_deactivated";
    $vars->{'flag_type'} = $flagtype;

    my @flagtypes = Bugzilla::FlagType->get_all;
    $vars->{'bug_types'} = [grep { $_->target_type eq 'bug' } @flagtypes];
    $vars->{'attachment_types'} = [grep { $_->target_type eq 'attachment' } @flagtypes];

    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

ThrowUserError('unknown_action', {action => $action});

#####################
# Helper subroutines
#####################

sub get_products_and_components {
    my $vars = {};
    my $user = Bugzilla->user;

    my @products;
    if ($user->in_group('editcomponents')) {
        if (Bugzilla->params->{useclassification}) {
            # We want products grouped by classifications.
            @products = map { @{ $_->products } } Bugzilla::Classification->get_all;
        }
        else {
            @products = Bugzilla::Product->get_all;
        }
    }
    else {
        @products = @{$user->get_products_by_permission('editcomponents')};

        if (Bugzilla->params->{useclassification}) {
            my %class;
            push(@{$class{$_->classification_id}}, $_) foreach @products;

            # Let's sort the list by classifications.
            @products = ();
            push(@products, @{$class{$_->id} || []}) foreach Bugzilla::Classification->get_all;
        }
    }

    my %components;
    foreach my $product (@products) {
        $components{$_->name} = 1 foreach @{$product->components};
    }
    $vars->{'products'} = \@products;
    $vars->{'components'} = [sort(keys %components)];
    return $vars;
}

sub get_editable_flagtypes {
    my ($products, $group_id) = @_;
    my $flagtypes;

    if (Bugzilla->user->in_group('editcomponents')) {
        $flagtypes = Bugzilla::FlagType::match({ group => $group_id });
        return $flagtypes;
    }

    my %visible_flagtypes;
    foreach my $product (@$products) {
        foreach my $target ('bug', 'attachment') {
            my $prod_flagtypes = $product->flag_types->{$target};
            $visible_flagtypes{$_->id} ||= $_ foreach @$prod_flagtypes;
        }
    }
    @$flagtypes = sort { $a->sortkey <=> $b->sortkey || $a->name cmp $b->name }
                    values %visible_flagtypes;
    # Filter flag types if a group ID is given.
    $flagtypes = filter_group($flagtypes, $group_id);
    return $flagtypes;
}

sub get_settable_groups {
    my $user = Bugzilla->user;
    my $groups = $user->in_group('editcomponents') ? [Bugzilla::Group->get_all] : $user->groups;
    return $groups;
}

sub filter_group {
    my ($flag_types, $gid) = @_;
    return $flag_types unless $gid;

    my @flag_types = grep {($_->grant_group && $_->grant_group->id == $gid)
                           || ($_->request_group && $_->request_group->id == $gid)} @$flag_types;

    return \@flag_types;
}

# Convert the array @clusions('prod_ID:comp_ID') back to a hash of
# the form %clusions{'prod_name:comp_name'} = 'prod_ID:comp_ID'
sub clusion_array_to_hash {
    my ($array, $visible_products) = @_;
    my $user = Bugzilla->user;
    my $has_privs = $user->in_group('editcomponents');

    my %hash;
    my %products;
    my %components;

    foreach my $ids (@$array) {
        my ($product_id, $component_id) = split(":", $ids);
        my $product_name = "__Any__";
        my $component_name = "__Any__";

        if ($product_id) {
            ($products{$product_id}) = grep { $_->id == $product_id } @$visible_products;
            next unless $products{$product_id};
            $product_name = $products{$product_id}->name;

            if ($component_id) {
                ($components{$component_id}) =
                  grep { $_->id == $component_id } @{$products{$product_id}->components};
                next unless $components{$component_id};
                $component_name = $components{$component_id}->name;
            }
        }
        else {
            # Users with local editcomponents privs cannot use __Any__:__Any__.
            next unless $has_privs;
            # It's illegal to select a component without a product.
            next if $component_id;
        }
        $hash{"$product_name:$component_name"} = $ids;
    }
    return \%hash;
}
