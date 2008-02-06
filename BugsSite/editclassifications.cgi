#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil; cperl-indent-level: 4 -*-
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
# The Initial Developer of the Original Code is Albert Ting
#
# Contributor(s): Albert Ting <alt@sonic.net>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#
# Direct any questions on this source code to mozilla.org

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Config qw($datadir);

require "CGI.pl";

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

# TestClassification:  just returns if the specified classification does exist
# CheckClassification: same check, optionally  emit an error text

sub TestClassification ($) {
    my $cl = shift;
    my $dbh = Bugzilla->dbh;

    trick_taint($cl);
    # does the classification exist?
    my $sth = $dbh->prepare("SELECT name
                             FROM classifications
                             WHERE name=?");
    $sth->execute($cl);
    my @row = $sth->fetchrow_array();
    return $row[0];
}

sub CheckClassification ($) {
    my $cl = shift;

    unless ($cl) {
        ThrowUserError("classification_not_specified");
    }
    if (! TestClassification($cl)) {
        ThrowUserError("classification_doesnt_exist", { name => $cl });
    }
}

sub LoadTemplate ($) {
    my $action = shift;

    $action =~ /(\w+)/;
    $action = $1;
    print $cgi->header();
    $template->process("admin/classifications/$action.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# Preliminary checks:
#

Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

exists Bugzilla->user->groups->{'editclassifications'}
  || ThrowUserError("auth_failure", {group  => "editclassifications",
                                     action => "edit",
                                     object => "classifications"});

ThrowUserError("auth_classification_not_enabled") unless Param("useclassification");

#
# often used variables
#
my $action = trim($cgi->param('action') || '');
my $classification = trim($cgi->param('classification') || '');
trick_taint($classification);
$vars->{'classification'} = $classification;

#
# action='' -> Show nice list of classifications
#

unless ($action) {
    my @classifications;
    # left join is tricky
    #   - must select "classifications" fields if you want a REAL value
    #   - must use "count(products.classification_id)" if you want a true
    #     count.  If you use count(classifications.id), it will return 1 for NULL
    #   - must use "group by classifications.id" instead of
    #     products.classification_id. Otherwise it won't look for all
    #     classification ids, just the ones used by the products.
    my $sth = $dbh->prepare("SELECT classifications.id, classifications.name,
                                    classifications.description,
                                    COUNT(classification_id) AS total
                             FROM classifications
                             LEFT JOIN products
                             ON classifications.id = products.classification_id
                            " . $dbh->sql_group_by('classifications.id',
                                         'classifications.name,
                                          classifications.description') . "
                             ORDER BY name");
    $sth->execute();
    while (my ($id,$classification,$description,$total) = $sth->fetchrow_array()) {
        my $cl = {};
        $cl->{'id'} = $id;
        $cl->{'classification'} = $classification;
        $cl->{'description'} = $description if (defined $description);
        $cl->{'total'} = $total;

        push(@classifications, $cl);
    }

    $vars->{'classifications'} = \@classifications;
    LoadTemplate("select");
}

#
# action='add' -> present form for parameters for new classification
#
# (next action will be 'new')
#

if ($action eq 'add') {
    LoadTemplate($action);
}

#
# action='new' -> add classification entered in the 'action=add' screen
#

if ($action eq 'new') {
    unless ($classification) {
        ThrowUserError("classification_not_specified");
    }
    if (TestClassification($classification)) {
        ThrowUserError("classification_already_exists", { name => $classification });
    }
    my $description = trim($cgi->param('description')  || '');
    trick_taint($description);

    # Add the new classification.
    my $sth = $dbh->prepare("INSERT INTO classifications (name,description)
                            VALUES (?,?)");
    $sth->execute($classification,$description);

    # Make versioncache flush
    unlink "$datadir/versioncache";

    LoadTemplate($action);
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    CheckClassification($classification);
    my $sth;

    # display some data about the classification
    $sth = $dbh->prepare("SELECT id, description
                          FROM classifications
                          WHERE name=?");
    $sth->execute($classification);
    my ($classification_id, $description) = $sth->fetchrow_array();

    ThrowUserError("classification_not_deletable") if ($classification_id eq "1");

    $sth = $dbh->prepare("SELECT name
                          FROM products
                          WHERE classification_id=$classification_id");
    $sth->execute();
    ThrowUserError("classification_has_products") if ($sth->fetchrow_array());

    $vars->{'description'} = $description if (defined $description);

    LoadTemplate($action);
}

#
# action='delete' -> really delete the classification
#

if ($action eq 'delete') {
    CheckClassification($classification);

    my $sth;
    my $classification_id = get_classification_id($classification);

    if ($classification_id == 1) {
        ThrowUserError("cant_delete_default_classification", { name => $classification });
    }

    # lock the tables before we start to change everything:
    $dbh->bz_lock_tables('classifications WRITE', 'products WRITE');

    # delete
    $sth = $dbh->prepare("DELETE FROM classifications WHERE id=?");
    $sth->execute($classification_id);

    # update products just in case
    $sth = $dbh->prepare("UPDATE products 
                          SET classification_id=1
                          WHERE classification_id=?");
    $sth->execute($classification_id);

    $dbh->bz_unlock_tables();

    unlink "$datadir/versioncache";

    LoadTemplate($action);
}

#
# action='edit' -> present the edit classifications from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    CheckClassification($classification);

    my @products = ();
    my $has_products = 0;
    my $sth;
    

    # get data of classification
    $sth = $dbh->prepare("SELECT id,description
                          FROM classifications
                          WHERE name=?");
    $sth->execute($classification);
    my ($classification_id,$description) = $sth->fetchrow_array();
    $vars->{'description'} = $description if (defined $description);

    $sth = $dbh->prepare("SELECT name,description
                          FROM products
                          WHERE classification_id=?
                          ORDER BY name");
    $sth->execute($classification_id);
    while ( my ($product, $prod_description) = $sth->fetchrow_array()) {
        my $prod = {};
        $has_products = 1;
        $prod->{'name'} = $product;
        $prod->{'description'} = $prod_description if (defined $prod_description);
        push(@products, $prod);
    }
    $vars->{'products'} = \@products if ($has_products);

    LoadTemplate($action);
}

#
# action='update' -> update the classification
#

if ($action eq 'update') {
    my $classificationold   = trim($cgi->param('classificationold')   || '');
    my $description         = trim($cgi->param('description')         || '');
    my $descriptionold      = trim($cgi->param('descriptionold')      || '');
    my $checkvotes = 0;
    my $sth;

    CheckClassification($classificationold);

    my $classification_id = get_classification_id($classificationold);
    trick_taint($description);

    # Note that we got the $classification_id using $classificationold
    # above so it will remain static even after we rename the
    # classification in the database.

    $dbh->bz_lock_tables('classifications WRITE');

    if ($classification ne $classificationold) {
        unless ($classification) {
            ThrowUserError("classification_not_specified");
        }
        
        if (TestClassification($classification)) {
            ThrowUserError("classification_already_exists", { name => $classification });
        }
        $sth = $dbh->prepare("UPDATE classifications
                              SET name=? WHERE id=?");
        $sth->execute($classification,$classification_id);
        $vars->{'updated_classification'} = 1;
    }

    if ($description ne $descriptionold) {
        $sth = $dbh->prepare("UPDATE classifications
                              SET description=?
                              WHERE id=?");
        $sth->execute($description,$classification_id);
        $vars->{'updated_description'} = 1;
    }

    $dbh->bz_unlock_tables();

    unlink "$datadir/versioncache";
    LoadTemplate($action);
}

#
# action='reclassify' -> reclassify products for the classification
#

if ($action eq 'reclassify') {
    CheckClassification($classification);
    my $sth;

    # display some data about the classification
    $sth = $dbh->prepare("SELECT id, description
                          FROM classifications
                          WHERE name=?");
    $sth->execute($classification);
    my ($classification_id, $description) = $sth->fetchrow_array();

    $vars->{'description'} = $description if (defined $description);

    $sth = $dbh->prepare("UPDATE products
                          SET classification_id=?
                          WHERE name=?");
    if (defined $cgi->param('add_products')) {
        if (defined $cgi->param('prodlist')) {
            foreach my $prod ($cgi->param("prodlist")) {
                trick_taint($prod);
                $sth->execute($classification_id,$prod);
            }
        }
    } elsif (defined $cgi->param('remove_products')) {
        if (defined $cgi->param('myprodlist')) {
            foreach my $prod ($cgi->param("myprodlist")) {
                trick_taint($prod);
                $sth->execute(1,$prod);
            }
        }
    } elsif (defined $cgi->param('migrate_products')) {
        if (defined $cgi->param('clprodlist')) {
            foreach my $prod ($cgi->param("clprodlist")) {
                trick_taint($prod);
                $sth->execute($classification_id,$prod);
            }
        }
    }

    my @selected_products = ();
    my @class_products = ();

    $sth = $dbh->prepare("SELECT classifications.id,
                                 products.name,
                                 classifications.name,
                                 classifications.id > 1 as unknown
                           FROM products
                           INNER JOIN classifications
                           ON classifications.id = products.classification_id
                           ORDER BY unknown, products.name,
                                    classifications.name");
    $sth->execute();
    while ( my ($clid, $name, $clname) = $sth->fetchrow_array() ) {
        if ($clid == $classification_id) {
            push(@selected_products,$name);
        } else {
            my $cl = {};
            if ($clid == 1) {
                $cl->{'name'} = "[$clname] $name";
            } else {
                $cl->{'name'} = "$name [$clname]";
            }
            $cl->{'value'} = $name;
            push(@class_products,$cl);
        }
    }
    $vars->{'selected_products'} = \@selected_products;
    $vars->{'class_products'} = \@class_products;

    LoadTemplate($action);
}

#
# No valid action found
#

ThrowCodeError("action_unrecognized", $vars);
