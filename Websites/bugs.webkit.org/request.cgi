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
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::User;
use Bugzilla::Product;
use Bugzilla::Component;

# Make sure the user is logged in.
my $user = Bugzilla->login();
my $cgi = Bugzilla->cgi;
# Force the script to run against the shadow DB. We already validated credentials.
Bugzilla->switch_to_shadow_db;
my $template = Bugzilla->template;
my $action = $cgi->param('action') || '';
my $format = $template->get_format('request/queue', 
                                   scalar($cgi->param('format')),
                                   scalar($cgi->param('ctype')));

$cgi->set_dated_content_disp("inline", "requests", $format->{extension});
print $cgi->header($format->{'ctype'});

my $fields;
$fields->{'requester'}->{'type'} = 'single';
# If the user doesn't restrict their search to requests from the wind
# (requestee ne '-'), include the requestee for completion.
unless (defined $cgi->param('requestee')
        && $cgi->param('requestee') eq '-')
{
    $fields->{'requestee'}->{'type'} = 'single';
}

Bugzilla::User::match_field($fields);

if ($action eq 'queue') {
    queue($format);
}
else {
    my $flagtypes = get_flag_types();
    my @types = ('all', @$flagtypes);

    my $vars = {};
    $vars->{'types'} = \@types;
    $vars->{'requests'} = {};

    my %components;
    foreach my $prod (@{$user->get_selectable_products}) {
        foreach my $comp (@{$prod->components}) {
            $components{$comp->name} = 1;
        }
    }
    $vars->{'components'} = [ sort { $a cmp $b } keys %components ];

    $template->process($format->{'template'}, $vars)
      || ThrowTemplateError($template->error());
}
exit;

################################################################################
# Functions
################################################################################

sub queue {
    my $format = shift;
    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;
    my $user = Bugzilla->user;
    my $userid = $user->id;
    my $vars = {};

    my $status = validateStatus($cgi->param('status'));
    my $form_group = validateGroup($cgi->param('group'));

    my $query = 
    # Select columns describing each flag, the bug/attachment on which
    # it has been set, who set it, and of whom they are requesting it.
    " SELECT    flags.id, flagtypes.name,
                flags.status,
                flags.bug_id, bugs.short_desc,
                products.name, components.name,
                flags.attach_id, attachments.description,
                requesters.realname, requesters.login_name,
                requestees.realname, requestees.login_name, COUNT(privs.group_id),
    " . $dbh->sql_date_format('flags.modification_date', '%Y.%m.%d %H:%i') .
    # Use the flags and flagtypes tables for information about the flags,
    # the bugs and attachments tables for target info, the profiles tables
    # for setter and requestee info, the products/components tables
    # so we can display product and component names, and the bug_group_map
    # table to help us weed out secure bugs to which the user should not have
    # access.
    "
      FROM           flags 
           LEFT JOIN attachments
                  ON flags.attach_id = attachments.attach_id
          INNER JOIN flagtypes
                  ON flags.type_id = flagtypes.id
          INNER JOIN profiles AS requesters
                  ON flags.setter_id = requesters.userid
           LEFT JOIN profiles AS requestees
                  ON flags.requestee_id  = requestees.userid
          INNER JOIN bugs
                  ON flags.bug_id = bugs.bug_id
          INNER JOIN products
                  ON bugs.product_id = products.id
          INNER JOIN components
                  ON bugs.component_id = components.id
           LEFT JOIN bug_group_map AS privs
                  ON privs.bug_id = bugs.bug_id
           LEFT JOIN cc AS ccmap
                  ON ccmap.who = $userid
                 AND ccmap.bug_id = bugs.bug_id
           LEFT JOIN bug_group_map AS bgmap
                  ON bgmap.bug_id = bugs.bug_id
    ";

    if (Bugzilla->params->{or_groups}) {
        $query .= " AND bgmap.group_id IN (" . $user->groups_as_string . ")";
        $query .= " WHERE     (privs.group_id IS NULL OR bgmap.group_id IS NOT NULL OR";
    }
    else {
        $query .= " AND bgmap.group_id NOT IN (" . $user->groups_as_string . ")";
        $query .= " WHERE     (bgmap.group_id IS NULL OR";
    }

    # Weed out bug the user does not have access to
    $query .=
    "            (ccmap.who IS NOT NULL AND cclist_accessible = 1) OR
                 (bugs.reporter = $userid AND bugs.reporter_accessible = 1) OR
                 (bugs.assigned_to = $userid) " .
                 (Bugzilla->params->{'useqacontact'} ? "OR
                 (bugs.qa_contact = $userid))" : ")");

    unless ($user->is_insider) {
        $query .= " AND (attachments.attach_id IS NULL
                         OR attachments.isprivate = 0
                         OR attachments.submitter_id = $userid)";
    }

    # Limit query to pending requests.
    $query .= " AND flags.status = '?' " unless $status;

    # The set of criteria by which we filter records to display in the queue.
    my @criteria = ();

    # A list of columns to exclude from the report because the report conditions
    # limit the data being displayed to exact matches for those columns.
    # In other words, if we are only displaying "pending" , we don't
    # need to display a "status" column in the report because the value for that
    # column will always be the same.
    my @excluded_columns = ();
    my $do_union = $cgi->param('do_union');

    # Filter results by exact email address of requester or requestee.
    if (defined $cgi->param('requester') && $cgi->param('requester') ne "") {
        my $requester = $dbh->quote($cgi->param('requester'));
        trick_taint($requester); # Quoted above
        push(@criteria, $dbh->sql_istrcmp('requesters.login_name', $requester));
        push(@excluded_columns, 'requester') unless $do_union;
    }
    if (defined $cgi->param('requestee') && $cgi->param('requestee') ne "") {
        if ($cgi->param('requestee') ne "-") {
            my $requestee = $dbh->quote($cgi->param('requestee'));
            trick_taint($requestee); # Quoted above
            push(@criteria, $dbh->sql_istrcmp('requestees.login_name', $requestee));
        }
        else {
            push(@criteria, "flags.requestee_id IS NULL");
        }
        push(@excluded_columns, 'requestee') unless $do_union;
    }

    # If the user wants requester = foo OR requestee = bar, we have to join
    # these criteria separately as all other criteria use AND.
    if (@criteria == 2 && $do_union) {
        my $union = join(' OR ', @criteria);
        @criteria = ("($union)");
    }

    # Filter requests by status: "pending", "granted", "denied", "all"
    # (which means any), or "fulfilled" (which means "granted" or "denied").
    if ($status) {
        if ($status eq "+-") {
            push(@criteria, "flags.status IN ('+', '-')");
            push(@excluded_columns, 'status');
        }
        elsif ($status ne "all") {
            push(@criteria, "flags.status = '$status'");
            push(@excluded_columns, 'status');
        }
    }

    # Filter results by exact product or component.
    if (defined $cgi->param('product') && $cgi->param('product') ne "") {
        my $product = Bugzilla::Product->check(scalar $cgi->param('product'));
        push(@criteria, "bugs.product_id = " . $product->id);
        push(@excluded_columns, 'product');
        if (defined $cgi->param('component') && $cgi->param('component') ne "") {
            my $component = Bugzilla::Component->check({ product => $product,
                                                         name => scalar $cgi->param('component') });
            push(@criteria, "bugs.component_id = " . $component->id);
            push(@excluded_columns, 'component');
        }
    }

    # Filter results by flag types.
    my $form_type = $cgi->param('type');
    if (defined $form_type && !grep($form_type eq $_, ("", "all"))) {
        # Check if any matching types are for attachments.  If not, don't show
        # the attachment column in the report.
        my $has_attachment_type =
            Bugzilla::FlagType::count({ 'name' => $form_type,
                                        'target_type' => 'attachment' });

        if (!$has_attachment_type) { push(@excluded_columns, 'attachment') }

        my $quoted_form_type = $dbh->quote($form_type);
        trick_taint($quoted_form_type); # Already SQL quoted
        push(@criteria, "flagtypes.name = " . $quoted_form_type);
        push(@excluded_columns, 'type');
    }

    $query .= ' AND ' . join(' AND ', @criteria) if scalar(@criteria);

    # Group the records by flag ID so we don't get multiple rows of data
    # for each flag.  This is only necessary because of the code that
    # removes flags on bugs the user is unauthorized to access.
    $query .= ' ' . $dbh->sql_group_by('flags.id',
               'flagtypes.name, flags.status, flags.bug_id, bugs.short_desc,
                products.name, components.name, flags.attach_id,
                attachments.description, requesters.realname,
                requesters.login_name, requestees.realname,
                requestees.login_name, flags.modification_date,
                cclist_accessible, bugs.reporter, bugs.reporter_accessible,
                bugs.assigned_to');

    # Group the records, in other words order them by the group column
    # so the loop in the display template can break them up into separate
    # tables every time the value in the group column changes.

    $form_group ||= "requestee";
    if ($form_group eq "requester") {
        $query .= " ORDER BY requesters.realname, requesters.login_name";
    }
    elsif ($form_group eq "requestee") {
        $query .= " ORDER BY requestees.realname, requestees.login_name";
    }
    elsif ($form_group eq "category") {
        $query .= " ORDER BY products.name, components.name";
    }
    elsif ($form_group eq "type") {
        $query .= " ORDER BY flagtypes.name";
    }

    # Order the records (within each group).
    $query .= " , flags.modification_date";

    # Pass the query to the template for use when debugging this script.
    $vars->{'query'} = $query;
    $vars->{'debug'} = $cgi->param('debug') ? 1 : 0;

    my $results = $dbh->selectall_arrayref($query);
    my @requests = ();
    foreach my $result (@$results) {
        my @data = @$result;
        my $request = {
          'id'              => $data[0] , 
          'type'            => $data[1] , 
          'status'          => $data[2] , 
          'bug_id'          => $data[3] , 
          'bug_summary'     => $data[4] , 
          'category'        => "$data[5]: $data[6]" , 
          'attach_id'       => $data[7] , 
          'attach_summary'  => $data[8] ,
          'requester'       => ($data[9] ? "$data[9] <$data[10]>" : $data[10]) , 
          'requestee'       => ($data[11] ? "$data[11] <$data[12]>" : $data[12]) , 
          'restricted'      => $data[13] ? 1 : 0,
          'created'         => $data[14]
        };
        push(@requests, $request);
    }

    # Get a list of request type names to use in the filter form.
    my @types = ("all");
    my $flagtypes = get_flag_types();
    push(@types, @$flagtypes);

    $vars->{'excluded_columns'} = \@excluded_columns;
    $vars->{'group_field'} = $form_group;
    $vars->{'requests'} = \@requests;
    $vars->{'types'} = \@types;

    # This code is needed to populate the Product and Component select fields.
    my ($products, %components);
    if (Bugzilla->params->{useclassification}) {
        foreach my $class (@{$user->get_selectable_classifications}) {
            push @$products, @{$user->get_selectable_products($class->id)};
        }
    }
    else {
        $products = $user->get_selectable_products;
    }

    foreach my $product (@$products) {
        $components{$_->name} = 1 foreach @{$product->components};
    }
    $vars->{'products'} = $products;
    $vars->{'components'} = [ sort keys %components ];

    $vars->{'urlquerypart'} = $cgi->canonicalise_query('ctype');

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process($format->{'template'}, $vars)
      || ThrowTemplateError($template->error());
}

################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateStatus {
    my $status = shift;
    return if !defined $status;

    grep($status eq $_, qw(? +- + - all))
      || ThrowUserError("flag_status_invalid", { status => $status });
    trick_taint($status);
    return $status;
}

sub validateGroup {
    my $group = shift;
    return if !defined $group;

    grep($group eq $_, qw(requester requestee category type))
      || ThrowUserError("request_queue_group_invalid", { group => $group });
    trick_taint($group);
    return $group;
}

# Returns all flag types which have at least one flag of this type.
# If a flag type is inactive but still has flags, we want it.
sub get_flag_types {
    my $dbh = Bugzilla->dbh;
    my $flag_types = $dbh->selectcol_arrayref('SELECT DISTINCT name
                                                 FROM flagtypes
                                                WHERE flagtypes.id IN
                                                      (SELECT DISTINCT type_id FROM flags)
                                             ORDER BY name');
    return $flag_types;
}
