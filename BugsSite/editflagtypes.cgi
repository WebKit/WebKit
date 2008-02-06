#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;
use lib ".";

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Use Bugzilla's flag modules for handling flag types.
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::User;

use vars qw( $template $vars );

# Make sure the user is logged in and is an administrator.
Bugzilla->login(LOGIN_REQUIRED);
UserInGroup("editcomponents")
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "flagtypes"});

# Suppress "used only once" warnings.
use vars qw(@legal_product @legal_components %components);

my $cgi = Bugzilla->cgi;
my $product_id;
my $component_id;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = $cgi->param('action') || 'list';
my @categoryActions;

if (@categoryActions = grep(/^categoryAction-.+/, $cgi->param())) {
    $categoryActions[0] =~ s/^categoryAction-//;
    processCategoryChange($categoryActions[0]);
    exit;
}

if    ($action eq 'list')           { list();           }
elsif ($action eq 'enter')          { edit();           }
elsif ($action eq 'copy')           { edit();           }
elsif ($action eq 'edit')           { edit();           }
elsif ($action eq 'insert')         { insert();         }
elsif ($action eq 'update')         { update();         }
elsif ($action eq 'confirmdelete')  { confirmDelete();  } 
elsif ($action eq 'delete')         { deleteType();     }
elsif ($action eq 'deactivate')     { deactivate();     }
else { 
    ThrowCodeError("action_unrecognized", { action => $action });
}

exit;

################################################################################
# Functions
################################################################################

sub list {
    # Define the variables and functions that will be passed to the UI template.
    $vars->{'bug_types'} = 
      Bugzilla::FlagType::match({ 'target_type' => 'bug', 
                                  'group' => scalar $cgi->param('group') }, 1);
    $vars->{'attachment_types'} = 
      Bugzilla::FlagType::match({ 'target_type' => 'attachment', 
                                  'group' => scalar $cgi->param('group') }, 1);

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub edit {
    $action eq 'enter' ? validateTargetType() : (my $id = validateID());
    
    # Get this installation's products and components.
    GetVersionTable();

    # products and components and the function used to modify the components
    # menu when the products menu changes; used by the template to populate
    # the menus and keep the components menu consistent with the products menu
    $vars->{'products'} = \@::legal_product;
    $vars->{'components'} = \@::legal_components;
    $vars->{'components_by_product'} = \%::components;
    
    $vars->{'last_action'} = $cgi->param('action');
    if ($cgi->param('action') eq 'enter' || $cgi->param('action') eq 'copy') {
        $vars->{'action'} = "insert";
    }
    else { 
        $vars->{'action'} = "update";
    }
    
    # If copying or editing an existing flag type, retrieve it.
    if ($cgi->param('action') eq 'copy' || $cgi->param('action') eq 'edit') { 
        $vars->{'type'} = Bugzilla::FlagType::get($id);
        $vars->{'type'}->{'inclusions'} = Bugzilla::FlagType::get_inclusions($id);
        $vars->{'type'}->{'exclusions'} = Bugzilla::FlagType::get_exclusions($id);
        # Users want to see group names, not IDs
        foreach my $group ("grant_gid", "request_gid") {
            my $gid = $vars->{'type'}->{$group};
            next if (!$gid);
            SendSQL("SELECT name FROM groups WHERE id = $gid");
            $vars->{'type'}->{$group} = FetchOneColumn();
        }
    }
    # Otherwise set the target type (the minimal information about the type
    # that the template needs to know) from the URL parameter and default
    # the list of inclusions to all categories.
    else {
        my %inclusions;
        $inclusions{"__Any__:__Any__"} = "0:0";
        $vars->{'type'} = { 'target_type' => scalar $cgi->param('target_type'),
                            'inclusions'  => \%inclusions };
    }
    
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub processCategoryChange {
    my $categoryAction = shift;
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    
    my @inclusions = $cgi->param('inclusions');
    my @exclusions = $cgi->param('exclusions');
    if ($categoryAction eq 'include') {
        validateProduct();
        validateComponent();
        my $category = ($product_id || 0) . ":" . ($component_id || 0);
        push(@inclusions, $category) unless grep($_ eq $category, @inclusions);
    }
    elsif ($categoryAction eq 'exclude') {
        validateProduct();
        validateComponent();
        my $category = ($product_id || 0) . ":" . ($component_id || 0);
        push(@exclusions, $category) unless grep($_ eq $category, @exclusions);
    }
    elsif ($categoryAction eq 'removeInclusion') {
        my @inclusion_to_remove = $cgi->param('inclusion_to_remove');
        @inclusions = map {(lsearch(\@inclusion_to_remove, $_) < 0) ? $_ : ()} @inclusions;
    }
    elsif ($categoryAction eq 'removeExclusion') {
        my @exclusion_to_remove = $cgi->param('exclusion_to_remove');
        @exclusions = map {(lsearch(\@exclusion_to_remove, $_) < 0) ? $_ : ()} @exclusions;
    }
    
    # Convert the array @clusions('prod_ID:comp_ID') back to a hash of
    # the form %clusions{'prod_name:comp_name'} = 'prod_ID:comp_ID'
    my %inclusions = clusion_array_to_hash(\@inclusions);
    my %exclusions = clusion_array_to_hash(\@exclusions);

    # Get this installation's products and components.
    GetVersionTable();

    # products and components; used by the template to populate the menus 
    # and keep the components menu consistent with the products menu
    $vars->{'products'} = \@::legal_product;
    $vars->{'components'} = \@::legal_components;
    $vars->{'components_by_product'} = \%::components;
    
    $vars->{'action'} = $cgi->param('action');
    my $type = {};
    foreach my $key ($cgi->param()) { $type->{$key} = $cgi->param($key) }
    $type->{'inclusions'} = \%inclusions;
    $type->{'exclusions'} = \%exclusions;
    $vars->{'type'} = $type;
    
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Convert the array @clusions('prod_ID:comp_ID') back to a hash of
# the form %clusions{'prod_name:comp_name'} = 'prod_ID:comp_ID'
sub clusion_array_to_hash {
    my $array = shift;
    my %hash;
    foreach my $ids (@$array) {
        trick_taint($ids);
        my ($product_id, $component_id) = split(":", $ids);
        my $product_name = get_product_name($product_id) || "__Any__";
        my $component_name = get_component_name($component_id) || "__Any__";
        $hash{"$product_name:$component_name"} = $ids;
    }
    return %hash;
}

sub insert {
    validateName();
    validateDescription();
    validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    validateGroups();

    my $dbh = Bugzilla->dbh;

    my $name = SqlQuote($cgi->param('name'));
    my $description = SqlQuote($cgi->param('description'));
    my $cc_list = SqlQuote($cgi->param('cc_list'));
    my $target_type = $cgi->param('target_type') eq "bug" ? "b" : "a";

    $dbh->bz_lock_tables('flagtypes WRITE', 'products READ',
                         'components READ', 'flaginclusions WRITE',
                         'flagexclusions WRITE');

    # Determine the new flag type's unique identifier.
    SendSQL("SELECT MAX(id) FROM flagtypes");
    my $id = FetchSQLData() + 1;
    
    # Insert a record for the new flag type into the database.
    SendSQL("INSERT INTO flagtypes (id, name, description, cc_list, 
                 target_type, sortkey, is_active, is_requestable, 
                 is_requesteeble, is_multiplicable, 
                 grant_group_id, request_group_id) 
             VALUES ($id, $name, $description, $cc_list, '$target_type', " .
                 $cgi->param('sortkey') . ", " .
                 $cgi->param('is_active') . ", " .
                 $cgi->param('is_requestable') . ", " .
                 $cgi->param('is_requesteeble') . ", " .
                 $cgi->param('is_multiplicable') . ", " .
                 $cgi->param('grant_gid') . ", " .
                 $cgi->param('request_gid') . ")");
    
    # Populate the list of inclusions/exclusions for this flag type.
    validateAndSubmit($id);

    $dbh->bz_unlock_tables();

    $vars->{'name'} = $cgi->param('name');
    $vars->{'message'} = "flag_type_created";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub update {
    my $id = validateID();
    validateName();
    validateDescription();
    validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    validateGroups();

    my $dbh = Bugzilla->dbh;

    my $name = SqlQuote($cgi->param('name'));
    my $description = SqlQuote($cgi->param('description'));
    my $cc_list = SqlQuote($cgi->param('cc_list'));

    $dbh->bz_lock_tables('flagtypes WRITE', 'products READ',
                         'components READ', 'flaginclusions WRITE',
                         'flagexclusions WRITE');
    SendSQL("UPDATE  flagtypes 
                SET  name = $name , 
                     description = $description , 
                     cc_list = $cc_list , 
                     sortkey = " . $cgi->param('sortkey') . ",
                     is_active = " . $cgi->param('is_active') . ",
                     is_requestable = " . $cgi->param('is_requestable') . ",
                     is_requesteeble = " . $cgi->param('is_requesteeble') . ",
                     is_multiplicable = " . $cgi->param('is_multiplicable') . ",
                     grant_group_id = " . $cgi->param('grant_gid') . ",
                     request_group_id = " . $cgi->param('request_gid') . "
              WHERE  id = $id");
    
    # Update the list of inclusions/exclusions for this flag type.
    validateAndSubmit($id);

    $dbh->bz_unlock_tables();
    
    # Clear existing flags for bugs/attachments in categories no longer on 
    # the list of inclusions or that have been added to the list of exclusions.
    SendSQL("
        SELECT flags.id 
        FROM flags
        INNER JOIN bugs
          ON flags.bug_id = bugs.bug_id
        LEFT OUTER JOIN flaginclusions AS i
          ON (flags.type_id = i.type_id 
            AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
            AND (bugs.component_id = i.component_id OR i.component_id IS NULL))
        WHERE flags.type_id = $id
        AND flags.is_active = 1
        AND i.type_id IS NULL
    ");
    Bugzilla::Flag::clear(FetchOneColumn()) while MoreSQLData();
    
    SendSQL("
        SELECT flags.id 
        FROM flags
        INNER JOIN bugs 
           ON flags.bug_id = bugs.bug_id
        INNER JOIN flagexclusions AS e
           ON flags.type_id = e.type_id
        WHERE flags.type_id = $id
        AND flags.is_active = 1
        AND (bugs.product_id = e.product_id OR e.product_id IS NULL)
        AND (bugs.component_id = e.component_id OR e.component_id IS NULL)
    ");
    Bugzilla::Flag::clear(FetchOneColumn()) while MoreSQLData();
    
    $vars->{'name'} = $cgi->param('name');
    $vars->{'message'} = "flag_type_changes_saved";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub confirmDelete 
{
  my $id = validateID();

  # check if we need confirmation to delete:
  
  my $count = Bugzilla::Flag::count({ 'type_id' => $id,
                                      'is_active' => 1 });
  
  if ($count > 0) {
    $vars->{'flag_type'} = Bugzilla::FlagType::get($id);
    $vars->{'flag_count'} = scalar($count);

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
  } 
  else {
    deleteType();
  }
}


sub deleteType {
    my $id = validateID();
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('flagtypes WRITE', 'flags WRITE',
                         'flaginclusions WRITE', 'flagexclusions WRITE');
    
    # Get the name of the flag type so we can tell users
    # what was deleted.
    SendSQL("SELECT name FROM flagtypes WHERE id = $id");
    $vars->{'name'} = FetchOneColumn();
    
    SendSQL("DELETE FROM flags WHERE type_id = $id");
    SendSQL("DELETE FROM flaginclusions WHERE type_id = $id");
    SendSQL("DELETE FROM flagexclusions WHERE type_id = $id");
    SendSQL("DELETE FROM flagtypes WHERE id = $id");
    $dbh->bz_unlock_tables();

    $vars->{'message'} = "flag_type_deleted";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub deactivate {
    my $id = validateID();
    validateIsActive();

    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('flagtypes WRITE');
    SendSQL("UPDATE flagtypes SET is_active = 0 WHERE id = $id");
    $dbh->bz_unlock_tables();
    
    $vars->{'message'} = "flag_type_deactivated";
    $vars->{'flag_type'} = Bugzilla::FlagType::get($id);
    
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateID {
    # $flagtype_id is destroyed if detaint_natural fails.
    my $flagtype_id = $cgi->param('id');
    detaint_natural($flagtype_id)
      || ThrowCodeError("flag_type_id_invalid",
                        { id => scalar $cgi->param('id') });

    SendSQL("SELECT 1 FROM flagtypes WHERE id = $flagtype_id");
    FetchOneColumn()
      || ThrowCodeError("flag_type_nonexistent", { id => $flagtype_id });

    return $flagtype_id;
}

sub validateName {
    $cgi->param('name')
      && $cgi->param('name') !~ /[ ,]/
      && length($cgi->param('name')) <= 50
      || ThrowUserError("flag_type_name_invalid",
                        { name => scalar $cgi->param('name') });
}

sub validateDescription {
    length($cgi->param('description')) < 2**16-1
      || ThrowUserError("flag_type_description_invalid");
}

sub validateCCList {
    length($cgi->param('cc_list')) <= 200
      || ThrowUserError("flag_type_cc_list_invalid", 
                        { cc_list => $cgi->param('cc_list') });
    
    my @addresses = split(/[, ]+/, $cgi->param('cc_list'));
    # We do not call Util::validate_email_syntax because these
    # addresses do not require to match 'emailregexp' and do not
    # depend on 'emailsuffix'. So we limit ourselves to a simple
    # sanity check:
    # - match the syntax of a fully qualified email address;
    # - do not contain any illegal character.
    foreach my $address (@addresses) {
        ($address =~ /^[\w\.\+\-=]+@[\w\.\-]+\.[\w\-]+$/
           && $address !~ /[\\\(\)<>&,;:"\[\] \t\r\n]/)
          || ThrowUserError('illegal_email_address',
                            {addr => $address, default => 1});
    }
}

sub validateProduct {
    return if !$cgi->param('product');
    
    $product_id = get_product_id($cgi->param('product'));
    
    defined($product_id)
      || ThrowCodeError("flag_type_product_nonexistent", 
                        { product => $cgi->param('product') });
}

sub validateComponent {
    return if !$cgi->param('component');
    
    $product_id
      || ThrowCodeError("flag_type_component_without_product");
    
    $component_id = get_component_id($product_id, $cgi->param('component'));

    defined($component_id)
      || ThrowCodeError("flag_type_component_nonexistent", 
                        { product   => $cgi->param('product'),
                          name => $cgi->param('component') });
}

sub validateSortKey {
    # $sortkey is destroyed if detaint_natural fails.
    my $sortkey = $cgi->param('sortkey');
    detaint_natural($sortkey)
      && $sortkey < 32768
      || ThrowUserError("flag_type_sortkey_invalid", 
                        { sortkey => scalar $cgi->param('sortkey') });
    $cgi->param('sortkey', $sortkey);
}

sub validateTargetType {
    grep($cgi->param('target_type') eq $_, ("bug", "attachment"))
      || ThrowCodeError("flag_type_target_type_invalid", 
                        { target_type => scalar $cgi->param('target_type') });
}

sub validateIsActive {
    $cgi->param('is_active', $cgi->param('is_active') ? 1 : 0);
}

sub validateIsRequestable {
    $cgi->param('is_requestable', $cgi->param('is_requestable') ? 1 : 0);
}

sub validateIsRequesteeble {
    $cgi->param('is_requesteeble', $cgi->param('is_requesteeble') ? 1 : 0);
}

sub validateAllowMultiple {
    $cgi->param('is_multiplicable', $cgi->param('is_multiplicable') ? 1 : 0);
}

sub validateGroups {
    # Convert group names to group IDs
    foreach my $col ("grant_gid", "request_gid") {
      my $name = $cgi->param($col);
      $cgi->param($col, "NULL") unless $name;
      next if (!$name);
      SendSQL("SELECT id FROM groups WHERE name = " . SqlQuote($name));
      my $gid = FetchOneColumn();
      if (!$gid) {
        ThrowUserError("group_unknown", { name => $name });
      }
      $cgi->param($col, $gid);
    }
}

# At this point, values either come the DB itself or have been recently
# added by the user and have passed all validation tests.
# The only way to have invalid product/component combinations is to
# hack the URL. So we silently ignore them, if any.
sub validateAndSubmit ($) {
    my ($id) = @_;
    my $dbh = Bugzilla->dbh;

    foreach my $category_type ("inclusions", "exclusions") {
        # Will be used several times below.
        my $sth = $dbh->prepare("INSERT INTO flag$category_type " .
                                "(type_id, product_id, component_id) " .
                                "VALUES (?, ?, ?)");

        $dbh->do("DELETE FROM flag$category_type WHERE type_id = ?", undef, $id);
        foreach my $category ($cgi->param($category_type)) {
            trick_taint($category);
            my ($product_id, $component_id) = split(":", $category);
            # The product does not exist.
            next if ($product_id && !get_product_name($product_id));
            # A component was selected without a product being selected.
            next if (!$product_id && $component_id);
            # The component does not belong to this product.
            next if ($component_id
                     && !$dbh->selectrow_array("SELECT id FROM components
                                                WHERE id = ? AND product_id = ?",
                                                undef, ($component_id, $product_id)));
            $product_id ||= undef;
            $component_id ||= undef;
            $sth->execute($id, $product_id, $component_id);
        }
    }
}
