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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Dave Lawrence <dkl@redhat.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

# Contains some global variables and routines used throughout bugzilla.

use strict;

use Bugzilla::DB qw(:DEFAULT :deprecated);
use Bugzilla::Constants;
use Bugzilla::Util;
# Bring ChmodDataFile in until this is all moved to the module
use Bugzilla::Config qw(:DEFAULT ChmodDataFile $localconfig $datadir);
use Bugzilla::BugMail;
use Bugzilla::User;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
    $zz = @main::default_column_list;
    $zz = @main::enterable_products;
    $zz = %main::keywordsbyname;
    $zz = @main::legal_bug_status;
    $zz = @main::legal_components;
    $zz = @main::legal_keywords;
    $zz = @main::legal_opsys;
    $zz = @main::legal_platform;
    $zz = @main::legal_priority;
    $zz = @main::legal_product;
    $zz = @main::legal_severity;
    $zz = @main::legal_target_milestone;
    $zz = @main::legal_versions;
    $zz = @main::milestoneurl;
    $zz = %main::proddesc;
    $zz = %main::classdesc;
    $zz = @main::prodmaxvotes;
    $zz = $main::template;
    $zz = $main::userid;
    $zz = $main::vars;
}

#
# Here are the --LOCAL-- variables defined in 'localconfig' that we'll use
# here
# 

# XXX - Move this to Bugzilla::Config once code which uses these has moved out
# of globals.pl
do $localconfig;

use DBI;

use Date::Format;               # For time2str().
use Date::Parse;               # For str2time().

# Use standard Perl libraries for cross-platform file/directory manipulation.
use File::Spec;
    
# Some environment variables are not taint safe
delete @::ENV{'PATH', 'IFS', 'CDPATH', 'ENV', 'BASH_ENV'};

# Cwd.pm in perl 5.6.1 gives a warning if $::ENV{'PATH'} isn't defined
# Set this to '' so that we don't get warnings cluttering the logs on every
# system call
$::ENV{'PATH'} = '';

# Ignore SIGTERM and SIGPIPE - this prevents DB corruption. If the user closes
# their browser window while a script is running, the webserver sends these
# signals, and we don't want to die half way through a write.
$::SIG{TERM} = 'IGNORE';
$::SIG{PIPE} = 'IGNORE';

# The following subroutine is for debugging purposes only.
# Uncommenting this sub and the $::SIG{__DIE__} trap underneath it will
# cause any fatal errors to result in a call stack trace to help track
# down weird errors.
#sub die_with_dignity {
#    use Carp;  # for confess()
#    my ($err_msg) = @_;
#    print $err_msg;
#    confess($err_msg);
#}
#$::SIG{__DIE__} = \&die_with_dignity;

sub GetFieldID {
    my ($f) = (@_);
    SendSQL("SELECT fieldid FROM fielddefs WHERE name = " . SqlQuote($f));
    my $fieldid = FetchOneColumn();
    die "Unknown field id: $f" if !$fieldid;
    return $fieldid;
}

# XXXX - this needs to go away
sub GenerateVersionTable {
    my $dbh = Bugzilla->dbh;

    SendSQL("SELECT versions.value, products.name " .
            "FROM versions, products " .
            "WHERE products.id = versions.product_id " .
            "ORDER BY versions.value");
    my @line;
    my %varray;
    my %carray;
    while (@line = FetchSQLData()) {
        my ($v,$p1) = (@line);
        if (!defined $::versions{$p1}) {
            $::versions{$p1} = [];
        }
        push @{$::versions{$p1}}, $v;
        $varray{$v} = 1;
    }
    SendSQL("SELECT components.name, products.name " .
            "FROM components, products " .
            "WHERE products.id = components.product_id " .
            "ORDER BY components.name");
    while (@line = FetchSQLData()) {
        my ($c,$p) = (@line);
        if (!defined $::components{$p}) {
            $::components{$p} = [];
        }
        my $ref = $::components{$p};
        push @$ref, $c;
        $carray{$c} = 1;
    }

    SendSQL("SELECT products.name, classifications.name " .
            "FROM products, classifications " .
            "WHERE classifications.id = products.classification_id " .
            "ORDER BY classifications.name");
    while (@line = FetchSQLData()) {
        my ($p,$c) = (@line);
        if (!defined $::classifications{$c}) {
            $::classifications{$c} = [];
        }
        my $ref = $::classifications{$c};
        push @$ref, $p;
    }

    my $dotargetmilestone = 1;  # This used to check the param, but there's
                                # enough code that wants to pretend we're using
                                # target milestones, even if they don't get
                                # shown to the user.  So we cache all the data
                                # about them anyway.

    my $mpart = $dotargetmilestone ? ", milestoneurl" : "";

    SendSQL("SELECT name, description FROM classifications ORDER BY name");
    while (@line = FetchSQLData()) {
        my ($n, $d) = (@line);
        $::classdesc{$n} = $d;
    }

    SendSQL("SELECT name, description, votesperuser, disallownew$mpart " .
            "FROM products ORDER BY name");
    while (@line = FetchSQLData()) {
        my ($p, $d, $votesperuser, $dis, $u) = (@line);
        $::proddesc{$p} = $d;
        if (!$dis && scalar($::components{$p})) {
            push @::enterable_products, $p;
        }
        if ($dotargetmilestone) {
            $::milestoneurl{$p} = $u;
        }
        $::prodmaxvotes{$p} = $votesperuser;
    }
            
    @::log_columns = $dbh->bz_table_columns('bugs');
    
    foreach my $i ("bug_id", "creation_ts", "delta_ts", "lastdiffed") {
        my $w = lsearch(\@::log_columns, $i);
        if ($w >= 0) {
            splice(@::log_columns, $w, 1);
        }
    }
    @::log_columns = (sort(@::log_columns));

    @::legal_priority   = get_legal_field_values("priority");
    @::legal_severity   = get_legal_field_values("bug_severity");
    @::legal_platform   = get_legal_field_values("rep_platform");
    @::legal_opsys      = get_legal_field_values("op_sys");
    @::legal_bug_status = get_legal_field_values("bug_status");
    @::legal_resolution = get_legal_field_values("resolution");

    # 'settable_resolution' is the list of resolutions that may be set 
    # directly by hand in the bug form. Start with the list of legal 
    # resolutions and remove 'MOVED' and 'DUPLICATE' because setting 
    # bugs to those resolutions requires a special process.
    #
    @::settable_resolution = @::legal_resolution;
    my $w = lsearch(\@::settable_resolution, "DUPLICATE");
    if ($w >= 0) {
        splice(@::settable_resolution, $w, 1);
    }
    my $z = lsearch(\@::settable_resolution, "MOVED");
    if ($z >= 0) {
        splice(@::settable_resolution, $z, 1);
    }

    my @list = sort { uc($a) cmp uc($b)} keys(%::versions);
    @::legal_product = @list;

    require File::Temp;
    my ($fh, $tmpname) = File::Temp::tempfile("versioncache.XXXXX",
                                              DIR => "$datadir");

    print $fh "#\n";
    print $fh "# DO NOT EDIT!\n";
    print $fh "# This file is automatically generated at least once every\n";
    print $fh "# hour by the GenerateVersionTable() sub in globals.pl.\n";
    print $fh "# Any changes you make will be overwritten.\n";
    print $fh "#\n";

    require Data::Dumper;
    print $fh (Data::Dumper->Dump([\@::log_columns, \%::versions],
                                  ['*::log_columns', '*::versions']));

    foreach my $i (@list) {
        if (!defined $::components{$i}) {
            $::components{$i} = [];
        }
    }
    @::legal_versions = sort {uc($a) cmp uc($b)} keys(%varray);
    print $fh (Data::Dumper->Dump([\@::legal_versions, \%::components],
                                  ['*::legal_versions', '*::components']));
    @::legal_components = sort {uc($a) cmp uc($b)} keys(%carray);

    print $fh (Data::Dumper->Dump([\@::legal_components, \@::legal_product,
                                   \@::legal_priority, \@::legal_severity,
                                   \@::legal_platform, \@::legal_opsys,
                                   \@::legal_bug_status, \@::legal_resolution],
                                  ['*::legal_components', '*::legal_product',
                                   '*::legal_priority', '*::legal_severity',
                                   '*::legal_platform', '*::legal_opsys',
                                   '*::legal_bug_status', '*::legal_resolution']));

    print $fh (Data::Dumper->Dump([\@::settable_resolution, \%::proddesc,
                                   \%::classifications, \%::classdesc,
                                   \@::enterable_products, \%::prodmaxvotes],
                                  ['*::settable_resolution', '*::proddesc',
                                   '*::classifications', '*::classdesc',
                                   '*::enterable_products', '*::prodmaxvotes']));

    if ($dotargetmilestone) {
        # reading target milestones in from the database - matthew@zeroknowledge.com
        SendSQL("SELECT milestones.value, products.name " .
                "FROM milestones, products " .
                "WHERE products.id = milestones.product_id " .
                "ORDER BY milestones.sortkey, milestones.value");
        my @line;
        my %tmarray;
        @::legal_target_milestone = ();
        while(@line = FetchSQLData()) {
            my ($tm, $pr) = (@line);
            if (!defined $::target_milestone{$pr}) {
                $::target_milestone{$pr} = [];
            }
            push @{$::target_milestone{$pr}}, $tm;
            if (!exists $tmarray{$tm}) {
                $tmarray{$tm} = 1;
                push(@::legal_target_milestone, $tm);
            }
        }

        print $fh (Data::Dumper->Dump([\%::target_milestone,
                                       \@::legal_target_milestone,
                                       \%::milestoneurl],
                                      ['*::target_milestone',
                                       '*::legal_target_milestone',
                                       '*::milestoneurl']));
    }

    SendSQL("SELECT id, name FROM keyworddefs ORDER BY name");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();
        push(@::legal_keywords, $name);
        $name = lc($name);
        $::keywordsbyname{$name} = $id;
    }

    print $fh (Data::Dumper->Dump([\@::legal_keywords, \%::keywordsbyname],
                                  ['*::legal_keywords', '*::keywordsbyname']));

    print $fh "1;\n";
    close $fh;

    rename ($tmpname, "$datadir/versioncache")
        || die "Can't rename $tmpname to versioncache";
    ChmodDataFile("$datadir/versioncache", 0666);
}


sub GetKeywordIdFromName {
    my ($name) = (@_);
    $name = lc($name);
    return $::keywordsbyname{$name};
}


$::VersionTableLoaded = 0;
sub GetVersionTable {
    return if $::VersionTableLoaded;
    my $mtime = file_mod_time("$datadir/versioncache");
    if (!defined $mtime || $mtime eq "" || !-r "$datadir/versioncache") {
        $mtime = 0;
    }
    if (time() - $mtime > 3600) {
        use Bugzilla::Token;
        Bugzilla::Token::CleanTokenTable() if Bugzilla->dbwritesallowed;
        GenerateVersionTable();
    }
    require "$datadir/versioncache";
    if (!defined %::versions) {
        GenerateVersionTable();
        do "$datadir/versioncache";

        if (!defined %::versions) {
            die "Can't generate file $datadir/versioncache";
        }
    }
    $::VersionTableLoaded = 1;
}

sub GenerateRandomPassword {
    my $size = (shift or 10); # default to 10 chars if nothing specified
    return join("", map{ ('0'..'9','a'..'z','A'..'Z')[rand 62] } (1..$size));
}

#
# This function checks if there are any entry groups defined.
# If called with no arguments, it identifies
# entry groups for all products.  If called with a product
# id argument, it checks for entry groups associated with 
# one particular product.
sub AnyEntryGroups {
    my $product_id = shift;
    $product_id = 0 unless ($product_id);
    return $::CachedAnyEntryGroups{$product_id} 
        if defined($::CachedAnyEntryGroups{$product_id});
    my $dbh = Bugzilla->dbh;
    PushGlobalSQLState();
    my $query = "SELECT 1 FROM group_control_map WHERE entry != 0";
    $query .= " AND product_id = $product_id" if ($product_id);
    $query .= " " . $dbh->sql_limit(1);
    SendSQL($query);
    if (MoreSQLData()) {
       $::CachedAnyEntryGroups{$product_id} = MoreSQLData();
       FetchSQLData();
       PopGlobalSQLState();
       return $::CachedAnyEntryGroups{$product_id};
    } else {
       return undef;
    }
}
#
# This function checks if there are any default groups defined.
# If so, then groups may have to be changed when bugs move from
# one bug to another.
sub AnyDefaultGroups {
    return $::CachedAnyDefaultGroups if defined($::CachedAnyDefaultGroups);
    my $dbh = Bugzilla->dbh;
    PushGlobalSQLState();
    SendSQL("SELECT 1 FROM group_control_map, groups WHERE " .
            "groups.id = group_control_map.group_id " .
            "AND isactive != 0 AND " .
            "(membercontrol = " . CONTROLMAPDEFAULT .
            " OR othercontrol = " . CONTROLMAPDEFAULT .
            ") " . $dbh->sql_limit(1));
    $::CachedAnyDefaultGroups = MoreSQLData();
    FetchSQLData();
    PopGlobalSQLState();
    return $::CachedAnyDefaultGroups;
}

#
# This function checks if, given a product id, the user can edit
# bugs in this product at all.
sub CanEditProductId {
    my ($productid) = @_;
    my $dbh = Bugzilla->dbh;
    my $query = "SELECT group_id FROM group_control_map " .
                "WHERE product_id = $productid " .
                "AND canedit != 0 "; 
    if (%{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= $dbh->sql_limit(1);
    PushGlobalSQLState();
    SendSQL($query);
    my ($result) = FetchSQLData();
    PopGlobalSQLState();
    return (!defined($result));
}

sub IsInClassification {
    my ($classification,$productname) = @_;

    if (! Param('useclassification')) {
        return 1;
    } else {
        my $query = "SELECT classifications.name " .
          "FROM products,classifications " .
            "WHERE products.classification_id=classifications.id ";
        $query .= "AND products.name = " . SqlQuote($productname);
        PushGlobalSQLState();
        SendSQL($query);
        my ($ret) = FetchSQLData();
        PopGlobalSQLState();
        return ($ret eq $classification);
    }
}

# This function determines whether or not a user can enter
# bugs into the named product.
sub CanEnterProduct {
    my ($productname, $verbose) = @_;
    my $dbh = Bugzilla->dbh;

    return unless defined($productname);
    trick_taint($productname);

    # First check whether or not the user has access to that product.
    my $query = "SELECT group_id IS NULL " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id " .
                "AND group_control_map.entry != 0 ";
    if (%{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE products.name = ? " .
              $dbh->sql_limit(1);

    my $has_access = $dbh->selectrow_array($query, undef, $productname);
    if (!$has_access) {
        # Do we require the exact reason why we cannot enter
        # bugs into that product? Returning -1 explicitely
        # means the user has no access to the product or the
        # product does not exist.
        return (defined($verbose)) ? -1 : 0;
    }

    # Check if the product is open for new bugs and has
    # at least one component and has at least one version.
    my ($allow_new_bugs, $has_version) = 
        $dbh->selectrow_array('SELECT CASE WHEN disallownew = 0 THEN 1 ELSE 0 END, ' .
                              'versions.value IS NOT NULL ' .
                              'FROM products INNER JOIN components ' .
                              'ON components.product_id = products.id ' .
                              'LEFT JOIN versions ' .
                              'ON versions.product_id = products.id ' .
                              'WHERE products.name = ? ' .
                              $dbh->sql_limit(1), undef, $productname);


    if (defined $verbose) {
        # Return (undef, undef) if the product has no components,
        # Return (?,     0)     if the product has no versions,
        # Return (0,     ?)     if the product is closed for new bug entry,
        # Return (1,     1)     if the user can enter bugs into the product,
        return ($allow_new_bugs, $has_version);
    } else {
        # Return undef if the product has no components
        # Return 0 if the product has no versions, or is closed for bug entry
        # Return 1 if the user can enter bugs into the product
        return ($allow_new_bugs && $has_version);
    }
}

# Call CanEnterProduct() and display an error message
# if the user cannot enter bugs into that product.
sub CanEnterProductOrWarn {
    my ($product) = @_;

    if (!defined($product)) {
        ThrowUserError("no_products");
    }
    my ($allow_new_bugs, $has_version) = CanEnterProduct($product, 1);
    trick_taint($product);

    if (!defined $allow_new_bugs) {
        ThrowUserError("missing_component", { product => $product });
    } elsif (!$allow_new_bugs) {
        ThrowUserError("product_disabled", { product => $product});
    } elsif ($allow_new_bugs < 0) {
        ThrowUserError("entry_access_denied", { product => $product});
    } elsif (!$has_version) {
        ThrowUserError("missing_version", { product => $product });
    }
    return 1;
}

sub GetEnterableProducts {
    my @products;
    # XXX rewrite into pure SQL instead of relying on legal_products?
    foreach my $p (@::legal_product) {
        if (CanEnterProduct($p)) {
            push @products, $p;
        }
    }
    return (@products);
}


#
# This function returns an alphabetical list of product names to which
# the user can enter bugs.  If the $by_id parameter is true, also retrieves IDs
# and pushes them onto the list as id, name [, id, name...] for easy slurping
# into a hash by the calling code.
sub GetSelectableProducts {
    my ($by_id,$by_classification) = @_;

    my $extra_sql = $by_id ? "id, " : "";

    my $extra_from_sql = $by_classification ? " INNER JOIN classifications"
        . " ON classifications.id = products.classification_id" : "";

    my $query = "SELECT $extra_sql products.name " .
                "FROM products $extra_from_sql " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
                  CONTROLMAPMANDATORY . " ";
    }
    if (%{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE group_id IS NULL ";
    if ($by_classification) {
        $query .= "AND classifications.name = ";
        $query .= SqlQuote($by_classification) . " ";
    }
    $query .= "ORDER BY name";
    PushGlobalSQLState();
    SendSQL($query);
    my @products = ();
    push(@products, FetchSQLData()) while MoreSQLData();
    PopGlobalSQLState();
    return (@products);
}

# GetSelectableProductHash
# returns a hash containing 
# legal_products => an enterable product list
# legal_(components|versions|milestones) =>
#   the list of components, versions, and milestones of enterable products
# (components|versions|milestones)_by_product
#    => a hash of component lists for each enterable product
# Milestones only get returned if the usetargetmilestones parameter is set.
sub GetSelectableProductHash {
    # The hash of selectable products and their attributes that gets returned
    # at the end of this function.
    my $selectables = {};

    my %products = GetSelectableProducts(1);

    $selectables->{legal_products} = [sort values %products];

    # Run queries that retrieve the list of components, versions,
    # and target milestones (if used) for the selectable products.
    my @tables = qw(components versions);
    push(@tables, 'milestones') if Param('usetargetmilestone');

    PushGlobalSQLState();
    foreach my $table (@tables) {
        my %values;
        my %values_by_product;

        if (scalar(keys %products)) {
            # Why oh why can't we standardize on these names?!?
            my $fld = ($table eq "components" ? "name" : "value");

            my $query = "SELECT $fld, product_id FROM $table WHERE product_id " .
                        "IN (" . join(",", keys %products) . ") ORDER BY $fld";
            SendSQL($query);

            while (MoreSQLData()) {
                my ($name, $product_id) = FetchSQLData();
                next unless $name;
                $values{$name} = 1;
                push @{$values_by_product{$products{$product_id}}}, $name;
            }
        }

        $selectables->{"legal_$table"} = [sort keys %values];
        $selectables->{"${table}_by_product"} = \%values_by_product;
    }
    PopGlobalSQLState();

    return $selectables;
}

#
# This function returns an alphabetical list of classifications that has products the user can enter bugs.
sub GetSelectableClassifications {
    my @selectable_classes = ();

    foreach my $c (sort keys %::classdesc) {
        if ( scalar(GetSelectableProducts(0,$c)) > 0) {
           push(@selectable_classes,$c);
        }
    }
    return (@selectable_classes);
}


sub ValidatePassword {
    # Determines whether or not a password is valid (i.e. meets Bugzilla's
    # requirements for length and content).    
    # If a second password is passed in, this function also verifies that
    # the two passwords match.
    my ($password, $matchpassword) = @_;
    
    if (length($password) < 3) {
        ThrowUserError("password_too_short");
    } elsif (length($password) > 16) {
        ThrowUserError("password_too_long");
    } elsif ((defined $matchpassword) && ($password ne $matchpassword)) {
        ThrowUserError("passwords_dont_match");
    }
}

sub DBID_to_name {
    my ($id) = (@_);
    return "__UNKNOWN__" if !defined $id;
    # $id should always be a positive integer
    if ($id =~ m/^([1-9][0-9]*)$/) {
        $id = $1;
    } else {
        $::cachedNameArray{$id} = "__UNKNOWN__";
    }
    if (!defined $::cachedNameArray{$id}) {
        PushGlobalSQLState();
        SendSQL("SELECT login_name FROM profiles WHERE userid = $id");
        my $r = FetchOneColumn();
        PopGlobalSQLState();
        if (!defined $r || $r eq "") {
            $r = "__UNKNOWN__";
        }
        $::cachedNameArray{$id} = $r;
    }
    return $::cachedNameArray{$id};
}

sub DBNameToIdAndCheck {
    my ($name) = (@_);
    my $result = login_to_id($name);
    if ($result > 0) {
        return $result;
    }

    ThrowUserError("invalid_username", { name => $name });
}

sub get_classification_id {
    my ($classification) = @_;
    PushGlobalSQLState();
    SendSQL("SELECT id FROM classifications WHERE name = " . SqlQuote($classification));
    my ($classification_id) = FetchSQLData();
    PopGlobalSQLState();
    return $classification_id;
}

sub get_classification_name {
    my ($classification_id) = @_;
    die "non-numeric classification_id '$classification_id' passed to get_classification_name"
      unless ($classification_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM classifications WHERE id = $classification_id");
    my ($classification) = FetchSQLData();
    PopGlobalSQLState();
    return $classification;
}



sub get_product_id {
    my ($prod) = @_;
    PushGlobalSQLState();
    SendSQL("SELECT id FROM products WHERE name = " . SqlQuote($prod));
    my ($prod_id) = FetchSQLData();
    PopGlobalSQLState();
    return $prod_id;
}

sub get_product_name {
    my ($prod_id) = @_;
    die "non-numeric prod_id '$prod_id' passed to get_product_name"
      unless ($prod_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM products WHERE id = $prod_id");
    my ($prod) = FetchSQLData();
    PopGlobalSQLState();
    return $prod;
}

sub get_component_id {
    my ($prod_id, $comp) = @_;
    return undef unless ($prod_id && ($prod_id =~ /^\d+$/));
    PushGlobalSQLState();
    SendSQL("SELECT id FROM components " .
            "WHERE product_id = $prod_id AND name = " . SqlQuote($comp));
    my ($comp_id) = FetchSQLData();
    PopGlobalSQLState();
    return $comp_id;
}

sub get_component_name {
    my ($comp_id) = @_;
    die "non-numeric comp_id '$comp_id' passed to get_component_name"
      unless ($comp_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM components WHERE id = $comp_id");
    my ($comp) = FetchSQLData();
    PopGlobalSQLState();
    return $comp;
}

# This routine quoteUrls contains inspirations from the HTML::FromText CPAN
# module by Gareth Rees <garethr@cre.canon.co.uk>.  It has been heavily hacked,
# all that is really recognizable from the original is bits of the regular
# expressions.
# This has been rewritten to be faster, mainly by substituting 'as we go'.
# If you want to modify this routine, read the comments carefully

sub quoteUrls {
    my ($text, $curr_bugid) = (@_);
    return $text unless $text;

    # We use /g for speed, but uris can have other things inside them
    # (http://foo/bug#3 for example). Filtering that out filters valid
    # bug refs out, so we have to do replacements.
    # mailto can't contain space or #, so we don't have to bother for that
    # Do this by escaping \0 to \1\0, and replacing matches with \0\0$count\0\0
    # \0 is used because its unliklely to occur in the text, so the cost of
    # doing this should be very small
    # Also, \0 won't appear in the value_quote'd bug title, so we don't have
    # to worry about bogus substitutions from there

    # escape the 2nd escape char we're using
    my $chr1 = chr(1);
    $text =~ s/\0/$chr1\0/g;

    # However, note that adding the title (for buglinks) can affect things
    # In particular, attachment matches go before bug titles, so that titles
    # with 'attachment 1' don't double match.
    # Dupe checks go afterwards, because that uses ^ and \Z, which won't occur
    # if it was subsituted as a bug title (since that always involve leading
    # and trailing text)

    # Because of entities, its easier (and quicker) to do this before escaping

    my @things;
    my $count = 0;
    my $tmp;

    # non-mailto protocols
    my $protocol_re = qr/(afs|cid|ftp|gopher|http|https|irc|mid|news|nntp|prospero|telnet|view-source|wais)/i;

    $text =~ s~\b(${protocol_re}:  # The protocol:
                  [^\s<>\"]+       # Any non-whitespace
                  [\w\/])          # so that we end in \w or /
              ~($tmp = html_quote($1)) &&
               ($things[$count++] = "<a href=\"$tmp\">$tmp</a>") &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egox;

    # We have to quote now, otherwise our html is itsself escaped
    # THIS MEANS THAT A LITERAL ", <, >, ' MUST BE ESCAPED FOR A MATCH

    $text = html_quote($text);

    # mailto:
    # Use |<nothing> so that $1 is defined regardless
    $text =~ s~\b(mailto:|)?([\w\.\-\+\=]+\@[\w\-]+(?:\.[\w\-]+)+)\b
              ~<a href=\"mailto:$2\">$1$2</a>~igx;

    # attachment links - handle both cases separately for simplicity
    $text =~ s~((?:^Created\ an\ |\b)attachment\s*\(id=(\d+)\)(\s\[edit\])?)
              ~($things[$count++] = GetAttachmentLink($2, $1)) &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egmx;

    $text =~ s~\b(attachment\s*\#?\s*(\d+))
              ~($things[$count++] = GetAttachmentLink($2, $1)) &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egmxi;

    # Current bug ID this comment belongs to
    my $current_bugurl = $curr_bugid ? "show_bug.cgi?id=$curr_bugid" : "";
    
    # This handles bug a, comment b type stuff. Because we're using /g
    # we have to do this in one pattern, and so this is semi-messy.
    # Also, we can't use $bug_re?$comment_re? because that will match the
    # empty string
    my $bug_re = qr/bug\s*\#?\s*(\d+)/i;
    my $comment_re = qr/comment\s*\#?\s*(\d+)/i;
    $text =~ s~\b($bug_re(?:\s*,?\s*$comment_re)?|$comment_re)
              ~ # We have several choices. $1 here is the link, and $2-4 are set
                # depending on which part matched
               (defined($2) ? GetBugLink($2,$1,$3) :
                              "<a href=\"$current_bugurl#c$4\">$1</a>")
              ~egox;

    # Duplicate markers
    $text =~ s~(?<=^\*\*\*\ This\ bug\ has\ been\ marked\ as\ a\ duplicate\ of\ )
               (\d+)
               (?=\ \*\*\*\Z)
              ~GetBugLink($1, $1)
              ~egmx;

    # Now remove the encoding hacks
    $text =~ s/\0\0(\d+)\0\0/$things[$1]/eg;
    $text =~ s/$chr1\0/\0/g;

    return $text;
}

# GetAttachmentLink creates a link to an attachment,
# including its title.

sub GetAttachmentLink {
    my ($attachid, $link_text) = @_;
    detaint_natural($attachid) ||
        die "GetAttachmentLink() called with non-integer attachment number";

    # If we've run GetAttachmentLink() for this attachment before,
    # %::attachlink will contain an anonymous array ref of relevant
    # values.  If not, we need to get the information from the database.
    if (! defined $::attachlink{$attachid}) {
        # Make sure any unfetched data from a currently running query
        # is saved off rather than overwritten
        PushGlobalSQLState();

        SendSQL("SELECT bug_id, isobsolete, description 
                 FROM attachments WHERE attach_id = $attachid");

        if (MoreSQLData()) {
            my ($bugid, $isobsolete, $desc) = FetchSQLData();
            my $title = "";
            my $className = "";
            if (Bugzilla->user->can_see_bug($bugid)) {
                $title = $desc;
            }
            if ($isobsolete) {
                $className = "bz_obsolete";
            }
            $::attachlink{$attachid} = [value_quote($title), $className];
        }
        else {
            # Even if there's nothing in the database, we want to save a blank
            # anonymous array in the %::attachlink hash so the query doesn't get
            # run again next time we're called for this attachment number.
            $::attachlink{$attachid} = [];
        }
        # All done with this sidetrip
        PopGlobalSQLState();
    }

    # Now that we know we've got all the information we're gonna get, let's
    # return the link (which is the whole reason we were called :)
    my ($title, $className) = @{$::attachlink{$attachid}};
    # $title will be undefined if the attachment didn't exist in the database.
    if (defined $title) {
        $link_text =~ s/ \[edit\]$//;
        my $linkval = "attachment.cgi?id=$attachid&amp;action=";
        # Whitespace matters here because these links are in <pre> tags.
        return qq|<span class="$className">|
               . qq|<a href="${linkval}view" title="$title">$link_text</a>|
               . qq| <a href="${linkval}review" title="$title">[review]</a>|
               . qq|</span>|;
    }
    else {
        return qq{$link_text};
    }
}

# GetBugLink creates a link to a bug, including its title.
# It takes either two or three parameters:
#  - The bug number
#  - The link text, to place between the <a>..</a>
#  - An optional comment number, for linking to a particular
#    comment in the bug

sub GetBugLink {
    my ($bug_num, $link_text, $comment_num) = @_;
    if (! defined $bug_num || $bug_num eq "") {
        return "&lt;missing bug number&gt;";
    }
    my $quote_bug_num = html_quote($bug_num);
    detaint_natural($bug_num) || return "&lt;invalid bug number: $quote_bug_num&gt;";

    # If we've run GetBugLink() for this bug number before, %::buglink
    # will contain an anonymous array ref of relevent values, if not
    # we need to get the information from the database.
    if (! defined $::buglink{$bug_num}) {
        # Make sure any unfetched data from a currently running query
        # is saved off rather than overwritten
        PushGlobalSQLState();

        SendSQL("SELECT bugs.bug_status, resolution, short_desc " .
                "FROM bugs WHERE bugs.bug_id = $bug_num");

        # If the bug exists, save its data off for use later in the sub
        if (MoreSQLData()) {
            my ($bug_state, $bug_res, $bug_desc) = FetchSQLData();
            # Initialize these variables to be "" so that we don't get warnings
            # if we don't change them below (which is highly likely).
            my ($pre, $title, $post) = ("", "", "");

            $title = $bug_state;
            if ($bug_state eq 'UNCONFIRMED') {
                $pre = "<i>";
                $post = "</i>";
            }
            elsif (! IsOpenedState($bug_state)) {
                $pre = '<span class="bz_closed">';
                $title .= " $bug_res";
                $post = '</span>';
            }
            if (Bugzilla->user->can_see_bug($bug_num)) {
                $title .= " - $bug_desc";
            }
            $::buglink{$bug_num} = [$pre, value_quote($title), $post];
        }
        else {
            # Even if there's nothing in the database, we want to save a blank
            # anonymous array in the %::buglink hash so the query doesn't get
            # run again next time we're called for this bug number.
            $::buglink{$bug_num} = [];
        }
        # All done with this sidetrip
        PopGlobalSQLState();
    }

    # Now that we know we've got all the information we're gonna get, let's
    # return the link (which is the whole reason we were called :)
    my ($pre, $title, $post) = @{$::buglink{$bug_num}};
    # $title will be undefined if the bug didn't exist in the database.
    if (defined $title) {
        my $linkval = "show_bug.cgi?id=$bug_num";
        if (defined $comment_num) {
            $linkval .= "#c$comment_num";
        }
        return qq{$pre<a href="$linkval" title="$title">$link_text</a>$post};
    }
    else {
        return qq{$link_text};
    }
}

sub GetLongDescriptionAsText {
    my ($id, $start, $end) = (@_);
    my $result = "";
    my $count = 0;
    my $anyprivate = 0;
    my $dbh = Bugzilla->dbh;
    my ($query) = ("SELECT profiles.login_name, " .
                   $dbh->sql_date_format('longdescs.bug_when', '%Y.%m.%d %H:%i') . ", " .
                   "       longdescs.thetext, longdescs.isprivate, " .
                   "       longdescs.already_wrapped " .
                   "FROM   longdescs, profiles " .
                   "WHERE  profiles.userid = longdescs.who " .
                   "AND    longdescs.bug_id = $id ");

    # $start will be undef for New bugs, and defined for pre-existing bugs.
    if ($start) {
        # If $start is not NULL, obtain the count-index
        # of this comment for the leading "Comment #xxx" line.)
        SendSQL("SELECT count(*) FROM longdescs " .
                " WHERE bug_id = $id AND bug_when <= '$start'");
        ($count) = (FetchSQLData());
         
        $query .= " AND longdescs.bug_when > '$start'"
                . " AND longdescs.bug_when <= '$end' ";
    }

    $query .= "ORDER BY longdescs.bug_when";
    SendSQL($query);
    while (MoreSQLData()) {
        my ($who, $when, $text, $isprivate, $work_time, $already_wrapped) = 
            (FetchSQLData());
        if ($count) {
            $result .= "\n\n------- Comment #$count from $who".Param('emailsuffix')."  ".
                Bugzilla::Util::format_time($when) . " -------\n";
        }
        if (($isprivate > 0) && Param("insidergroup")) {
            $anyprivate = 1;
        }
        $result .= ($already_wrapped ? $text : wrap_comment($text));
        $count++;
    }

    return ($result, $anyprivate);
}

# Returns a list of all the legal values for a field that has a
# list of legal values, like rep_platform or resolution.
sub get_legal_field_values {
    my ($field) = @_;
    my $dbh = Bugzilla->dbh;
    my $result_ref = $dbh->selectcol_arrayref(
         "SELECT value FROM $field
           WHERE isactive = ?
        ORDER BY sortkey, value", undef, (1));
    return @$result_ref;
}

sub BugInGroupId {
    my ($bugid, $groupid) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT bug_id != 0 FROM bug_group_map
            WHERE bug_id = $bugid
            AND group_id = $groupid");
    my $bugingroup = FetchOneColumn();
    PopGlobalSQLState();
    return $bugingroup;
}

sub GroupExists {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT id FROM groups WHERE name=" . SqlQuote($groupname));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    return $id;
}

sub GroupNameToId {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT id FROM groups WHERE name=" . SqlQuote($groupname));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    return $id;
}

sub GroupIdToName {
    my ($groupid) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM groups WHERE id = $groupid");
    my $name = FetchOneColumn();
    PopGlobalSQLState();
    return $name;
}


# Determines whether or not a group is active by checking 
# the "isactive" column for the group in the "groups" table.
# Note: This function selects groups by id rather than by name.
sub GroupIsActive {
    my ($groupid) = (@_);
    $groupid ||= 0;
    PushGlobalSQLState();
    SendSQL("SELECT isactive FROM groups WHERE id=$groupid");
    my $isactive = FetchOneColumn();
    PopGlobalSQLState();
    return $isactive;
}

# Determines if the given bug_status string represents an "Opened" bug.  This
# routine ought to be parameterizable somehow, as people tend to introduce
# new states into Bugzilla.

sub IsOpenedState {
    my ($state) = (@_);
    if (grep($_ eq $state, OpenStates())) {
        return 1;
    }
    return 0;
}

# This sub will return an array containing any status that
# is considered an open bug.

sub OpenStates {
    return ('NEW', 'REOPENED', 'ASSIGNED', 'UNCONFIRMED');
}


###############################################################################

# Constructs a format object from URL parameters. You most commonly call it 
# like this:
# my $format = GetFormat("foo/bar", scalar($cgi->param('format')),
#                        scalar($cgi->param('ctype')));

sub GetFormat {
    my ($template, $format, $ctype) = @_;

    $ctype ||= "html";
    $format ||= "";

    # Security - allow letters and a hyphen only
    $ctype =~ s/[^a-zA-Z\-]//g;
    $format =~ s/[^a-zA-Z\-]//g;
    trick_taint($ctype);
    trick_taint($format);

    $template .= ($format ? "-$format" : "");
    $template .= ".$ctype.tmpl";

    # Now check that the template actually exists. We only want to check
    # if the template exists; any other errors (eg parse errors) will
    # end up being detected later.
    eval {
        Bugzilla->template->context->template($template);
    };
    # This parsing may seem fragile, but its OK:
    # http://lists.template-toolkit.org/pipermail/templates/2003-March/004370.html
    # Even if it is wrong, any sort of error is going to cause a failure
    # eventually, so the only issue would be an incorrect error message
    if ($@ && $@->info =~ /: not found$/) {
        ThrowUserError("format_not_found", { 'format' => $format,
                                             'ctype' => $ctype,
                                           });
    }

    # Else, just return the info
    return
    {
        'template'    => $template ,
        'extension'   => $ctype ,
        'ctype'       => Bugzilla::Constants::contenttypes->{$ctype} ,
    };
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

$::template = Bugzilla->template();

$::vars = {};

1;
