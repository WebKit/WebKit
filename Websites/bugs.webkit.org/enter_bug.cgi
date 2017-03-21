#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

##############################################################################
#
# enter_bug.cgi
# -------------
# Displays bug entry form. Bug fields are specified through popup menus, 
# drop-down lists, or text fields. Default for these values can be 
# passed in as parameters to the cgi.
#
##############################################################################

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Bug;
use Bugzilla::Hook;
use Bugzilla::Classification;
use Bugzilla::Token;
use Bugzilla::Field;
use Bugzilla::Status;
use Bugzilla::UserAgent;

use List::MoreUtils qw(none);

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cloned_bug;
my $cloned_bug_id;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

# All pages point to the same part of the documentation.
$vars->{'doc_section'} = 'using/filing.html';

my $product_name = trim($cgi->param('product') || '');
# Will contain the product object the bug is created in.
my $product;

if ($product_name eq '') {
    # If the user cannot enter bugs in any product, stop here.
    my @enterable_products = @{$user->get_enterable_products};
    ThrowUserError('no_products') unless scalar(@enterable_products);

    my $classification = Bugzilla->params->{'useclassification'} ?
        scalar($cgi->param('classification')) : '__all';

    # Unless a real classification name is given, we sort products
    # by classification.
    my @classifications;

    unless ($classification && $classification ne '__all') {
        @classifications = @{sort_products_by_classification(\@enterable_products)};
    }

    unless ($classification) {
        # We know there is at least one classification available,
        # else we would have stopped earlier.
        if (scalar(@classifications) > 1) {
            # We only need classification objects.
            $vars->{'classifications'} = [map {$_->{'object'}} @classifications];

            $vars->{'target'} = "enter_bug.cgi";

            print $cgi->header();
            $template->process("global/choose-classification.html.tmpl", $vars)
               || ThrowTemplateError($template->error());
            exit;
        }
        # If we come here, then there is only one classification available.
        $classification = $classifications[0]->{'object'}->name;
    }

    # Keep only enterable products which are in the specified classification.
    if ($classification ne "__all") {
        my $class = new Bugzilla::Classification({'name' => $classification});
        # If the classification doesn't exist, then there is no product in it.
        if ($class) {
            @enterable_products
              = grep {$_->classification_id == $class->id} @enterable_products;
            @classifications = ({object => $class, products => \@enterable_products});
        }
        else {
            @enterable_products = ();
        }
    }

    if (scalar(@enterable_products) == 0) {
        ThrowUserError('no_products');
    }
    elsif (scalar(@enterable_products) > 1) {
        $vars->{'classifications'} = \@classifications;
        $vars->{'target'} = "enter_bug.cgi";

        print $cgi->header();
        $template->process("global/choose-product.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    } else {
        # Only one product exists.
        $product = $enterable_products[0];
    }
}

# We need to check and make sure that the user has permission
# to enter a bug against this product.
$product = $user->can_enter_product($product || $product_name, THROW_ERROR);

##############################################################################
# Useful Subroutines
##############################################################################
sub formvalue {
    my ($name, $default) = (@_);
    return Bugzilla->cgi->param($name) || $default || "";
}

##############################################################################
# End of subroutines
##############################################################################

my $has_editbugs = $user->in_group('editbugs', $product->id);
my $has_canconfirm = $user->in_group('canconfirm', $product->id);

# If a user is trying to clone a bug
#   Check that the user has authorization to view the parent bug
#   Create an instance of Bug that holds the info from the parent
$cloned_bug_id = $cgi->param('cloned_bug_id');

if ($cloned_bug_id) {
    $cloned_bug = Bugzilla::Bug->check($cloned_bug_id);
    $cloned_bug_id = $cloned_bug->id;
}

# If there is only one active component, choose it
my @active = grep { $_->is_active } @{$product->components};
if (scalar(@active) == 1) {
    $cgi->param('component', $active[0]->name);
}

# If there is only one active version, choose it
@active = grep { $_->is_active } @{$product->versions};
if (scalar(@active) == 1) {
    $cgi->param('version', $active[0]->name);
}

my %default;

$vars->{'product'}               = $product;

$vars->{'priority'}              = get_legal_field_values('priority');
$vars->{'bug_severity'}          = get_legal_field_values('bug_severity');
$vars->{'rep_platform'}          = get_legal_field_values('rep_platform');
$vars->{'op_sys'}                = get_legal_field_values('op_sys');

$vars->{'assigned_to'}           = formvalue('assigned_to');
$vars->{'assigned_to_disabled'}  = !$has_editbugs;
$vars->{'cc_disabled'}           = 0;

$vars->{'qa_contact'}           = formvalue('qa_contact');
$vars->{'qa_contact_disabled'}  = !$has_editbugs;

$vars->{'cloned_bug_id'}         = $cloned_bug_id;

$vars->{'token'} = issue_session_token('create_bug');


my @enter_bug_fields = grep { $_->enter_bug } Bugzilla->active_custom_fields;
foreach my $field (@enter_bug_fields) {
    my $cf_name = $field->name;
    my $cf_value = $cgi->param($cf_name);
    if (defined $cf_value) {
        if ($field->type == FIELD_TYPE_MULTI_SELECT) {
            $cf_value = [$cgi->param($cf_name)];
        }
        $default{$cf_name} = $vars->{$cf_name} = $cf_value;
    }
}

# This allows the Field visibility and value controls to work with the
# Classification and Product fields as a parent.
$default{'classification'} = $product->classification->name;
$default{'product'} = $product->name;

if ($cloned_bug_id) {

    $default{'component_'}    = $cloned_bug->component;
    $default{'priority'}      = $cloned_bug->priority;
    $default{'bug_severity'}  = $cloned_bug->bug_severity;
    $default{'rep_platform'}  = $cloned_bug->rep_platform;
    $default{'op_sys'}        = $cloned_bug->op_sys;

    $vars->{'short_desc'}     = $cloned_bug->short_desc;
    $vars->{'bug_file_loc'}   = $cloned_bug->bug_file_loc;
    $vars->{'keywords'}       = $cloned_bug->keywords;
    $vars->{'dependson'}      = join (", ", $cloned_bug_id, @{$cloned_bug->dependson});
    $vars->{'blocked'}        = join (", ", @{$cloned_bug->blocked});
    $vars->{'deadline'}       = $cloned_bug->deadline;
    $vars->{'estimated_time'} = $cloned_bug->estimated_time;

    if (scalar @{$cloned_bug->cc}) {
        $vars->{'cc'}         = join (", ", @{$cloned_bug->cc});
    } else {
        $vars->{'cc'}         = formvalue('cc');
    }

    foreach my $role (qw(reporter assigned_to qa_contact)) {
        if (defined($cloned_bug->$role)
            && $cloned_bug->$role->id != $user->id
            && none { $_ eq $cloned_bug->$role->login } @{$cloned_bug->cc})
        {
            $vars->{'cc'} = join (", ", $cloned_bug->$role->login, $vars->{'cc'});
        }
    }

    foreach my $field (@enter_bug_fields) {
        my $field_name = $field->name;
        $vars->{$field_name} = $cloned_bug->$field_name;
    }

    # We need to ensure that we respect the 'insider' status of
    # the first comment, if it has one. Either way, make a note
    # that this bug was cloned from another bug.
    my $bug_desc = $cloned_bug->comments({ order => 'oldest_to_newest' })->[0];
    my $isprivate = $bug_desc->is_private;

    $vars->{'comment'} = "";
    $vars->{'comment_is_private'} = 0;

    if (!$isprivate || $user->is_insider) {
        # We use "body" to avoid any format_comment text, which would be
        # pointless to clone.
        $vars->{'comment'} = $bug_desc->body;
        $vars->{'comment_is_private'} = $isprivate;
    }

} # end of cloned bug entry form

else {
    $default{'component_'}    = formvalue('component');
    $default{'priority'}      = formvalue('priority', Bugzilla->params->{'defaultpriority'});
    $default{'bug_severity'}  = formvalue('bug_severity', Bugzilla->params->{'defaultseverity'});
    $default{'rep_platform'}  = formvalue('rep_platform', 
                                          Bugzilla->params->{'defaultplatform'} || detect_platform());
    $default{'op_sys'}        = formvalue('op_sys', 
                                          Bugzilla->params->{'defaultopsys'} || detect_op_sys());

    $vars->{'alias'}          = formvalue('alias');
    $vars->{'short_desc'}     = formvalue('short_desc');
    $vars->{'bug_file_loc'}   = formvalue('bug_file_loc', "http://");
    $vars->{'keywords'}       = formvalue('keywords');
    $vars->{'dependson'}      = formvalue('dependson');
    $vars->{'blocked'}        = formvalue('blocked');
    $vars->{'deadline'}       = formvalue('deadline');
    $vars->{'estimated_time'} = formvalue('estimated_time');
    $vars->{'see_also'}       = formvalue('see_also');

    $vars->{'cc'}             = join(', ', $cgi->param('cc'));

    $vars->{'comment'}        = formvalue('comment');
    $vars->{'comment_is_private'} = formvalue('comment_is_private');

} # end of normal/bookmarked entry form


# IF this is a cloned bug,
# AND the clone's product is the same as the parent's
#   THEN use the version from the parent bug
# ELSE IF a version is supplied in the URL
#   THEN use it
# ELSE IF there is a version in the cookie
#   THEN use it (Posting a bug sets a cookie for the current version.)
# ELSE
#   The default version is the last one in the list (which, it is
#   hoped, will be the most recent one).
#
# Eventually maybe each product should have a "current version"
# parameter.
$vars->{'version'} = $product->versions;

my $version_cookie = $cgi->cookie("VERSION-" . $product->name);

if ( ($cloned_bug_id) &&
     ($product->name eq $cloned_bug->product ) ) {
    $default{'version'} = $cloned_bug->version;
} elsif (formvalue('version')) {
    $default{'version'} = formvalue('version');
} elsif (defined $version_cookie
         and grep { $_->name eq $version_cookie } @{ $vars->{'version'} })
{
    $default{'version'} = $version_cookie;
} else {
    $default{'version'} = $vars->{'version'}->[$#{$vars->{'version'}}]->name;
}

# Get list of milestones.
if ( Bugzilla->params->{'usetargetmilestone'} ) {
    $vars->{'target_milestone'} = $product->milestones;
    if (formvalue('target_milestone')) {
       $default{'target_milestone'} = formvalue('target_milestone');
    } else {
       $default{'target_milestone'} = $product->default_milestone;
    }
}

# Construct the list of allowable statuses.
my @statuses = @{ Bugzilla::Bug->new_bug_statuses($product) };
# Exclude closed states from the UI, even if the workflow allows them.
# The back-end code will still accept them, though.
# XXX We should remove this when the UI accepts closed statuses and update
# Bugzilla::Bug->default_bug_status.
@statuses = grep { $_->is_open } @statuses;

scalar(@statuses) || ThrowUserError('no_initial_bug_status');

$vars->{'bug_status'} = \@statuses;

# Get the default from a template value if it is legitimate.
# Otherwise, and only if the user has privs, set the default
# to the first confirmed bug status on the list, if available.

my $picked_status = formvalue('bug_status');
if ($picked_status and grep($_->name eq $picked_status, @statuses)) {
    $default{'bug_status'} = formvalue('bug_status');
} else {
    $default{'bug_status'} = Bugzilla::Bug->default_bug_status(@statuses);
}

my @groups = $cgi->param('groups');
if ($cloned_bug) {
    my @clone_groups = map { $_->name } @{ $cloned_bug->groups_in };
    # It doesn't matter if there are duplicate names, since all we check
    # for in the template is whether or not the group is set.
    push(@groups, @clone_groups);
}
$default{'groups'} = \@groups;

Bugzilla::Hook::process('enter_bug_entrydefaultvars', { vars => $vars });

$vars->{'default'} = \%default;

my $format = $template->get_format("bug/create/create",
                                   scalar $cgi->param('format'), 
                                   scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());          
