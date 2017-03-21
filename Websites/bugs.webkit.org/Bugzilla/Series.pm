# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module implements a series - a set of data to be plotted on a chart.
#
# This Series is in the database if and only if self->{'series_id'} is defined. 
# Note that the series being in the database does not mean that the fields of 
# this object are the same as the DB entries, as the object may have been 
# altered.

package Bugzilla::Series;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Error;
use Bugzilla::Util;

# This is a hack so that we can re-use the rename_field_value
# code from Bugzilla::Search::Saved.
use constant DB_TABLE => 'series';
use constant ID_FIELD => 'series_id';

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    # Create a ref to an empty hash and bless it
    my $self = {};
    bless($self, $class);

    my $arg_count = scalar(@_);
    
    # new() can return undef if you pass in a series_id and the user doesn't 
    # have sufficient permissions. If you create a new series in this way,
    # you need to check for an undef return, and act appropriately.
    my $retval = $self;

    # There are three ways of creating Series objects. Two (CGI and Parameters)
    # are for use when creating a new series. One (Database) is for retrieving
    # information on existing series.
    if ($arg_count == 1) {
        if (ref($_[0])) {
            # We've been given a CGI object to create a new Series from.
            # This series may already exist - external code needs to check
            # before it calls writeToDatabase().
            $self->initFromCGI($_[0]);
        }
        else {
            # We've been given a series_id, which should represent an existing
            # Series.
            $retval = $self->initFromDatabase($_[0]);
        }
    }
    elsif ($arg_count >= 6 && $arg_count <= 8) {
        # We've been given a load of parameters to create a new Series from.
        # Currently, undef is always passed as the first parameter; this allows
        # you to call writeToDatabase() unconditionally.
        # XXX - You cannot set category_id and subcategory_id from here.
        $self->initFromParameters(@_);
    }
    else {
        die("Bad parameters passed in - invalid number of args: $arg_count");
    }

    return $retval;
}

sub initFromDatabase {
    my ($self, $series_id) = @_;
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->user;

    detaint_natural($series_id) 
      || ThrowCodeError("invalid_series_id", { 'series_id' => $series_id });

    my $grouplist = $user->groups_as_string;

    my @series = $dbh->selectrow_array("SELECT series.series_id, cc1.name, " .
        "cc2.name, series.name, series.creator, series.frequency, " .
        "series.query, series.is_public, series.category, series.subcategory " .
        "FROM series " .
        "INNER JOIN series_categories AS cc1 " .
        "    ON series.category = cc1.id " .
        "INNER JOIN series_categories AS cc2 " .
        "    ON series.subcategory = cc2.id " .
        "LEFT JOIN category_group_map AS cgm " .
        "    ON series.category = cgm.category_id " .
        "    AND cgm.group_id NOT IN($grouplist) " .
        "WHERE series.series_id = ? " .
        "    AND (creator = ? OR (is_public = 1 AND cgm.category_id IS NULL))",
        undef, ($series_id, $user->id));

    if (@series) {
        $self->initFromParameters(@series);
        return $self;
    }
    else {
        return undef;
    }
}

sub initFromParameters {
    # Pass undef as the first parameter if you are creating a new series.
    my $self = shift;

    ($self->{'series_id'}, $self->{'category'},  $self->{'subcategory'},
     $self->{'name'}, $self->{'creator_id'}, $self->{'frequency'},
     $self->{'query'}, $self->{'public'}, $self->{'category_id'},
     $self->{'subcategory_id'}) = @_;

    # If the first parameter is undefined, check if this series already
    # exists and update it series_id accordingly
    $self->{'series_id'} ||= $self->existsInDatabase();
}

sub initFromCGI {
    my $self = shift;
    my $cgi = shift;

    $self->{'series_id'} = $cgi->param('series_id') || undef;
    if (defined($self->{'series_id'})) {
        detaint_natural($self->{'series_id'})
          || ThrowCodeError("invalid_series_id", 
                               { 'series_id' => $self->{'series_id'} });
    }
    
    $self->{'category'} = $cgi->param('category')
      || $cgi->param('newcategory')
      || ThrowUserError("missing_category");

    $self->{'subcategory'} = $cgi->param('subcategory')
      || $cgi->param('newsubcategory')
      || ThrowUserError("missing_subcategory");

    $self->{'name'} = $cgi->param('name')
      || ThrowUserError("missing_name");

    $self->{'creator_id'} = Bugzilla->user->id;

    $self->{'frequency'} = $cgi->param('frequency');
    detaint_natural($self->{'frequency'})
      || ThrowUserError("missing_frequency");

    $self->{'query'} = $cgi->canonicalise_query("format", "ctype", "action",
                                        "category", "subcategory", "name",
                                        "frequency", "public", "query_format");
    trick_taint($self->{'query'});
                                        
    $self->{'public'} = $cgi->param('public') ? 1 : 0;
    
    # Change 'admin' here and in series.html.tmpl, or remove the check
    # completely, if you want to change who can make series public.
    $self->{'public'} = 0 unless Bugzilla->user->in_group('admin');
}

sub writeToDatabase {
    my $self = shift;

    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();

    my $category_id = getCategoryID($self->{'category'});
    my $subcategory_id = getCategoryID($self->{'subcategory'});

    my $exists;
    if ($self->{'series_id'}) { 
        $exists = 
            $dbh->selectrow_array("SELECT series_id FROM series
                                   WHERE series_id = $self->{'series_id'}");
    }
    
    # Is this already in the database?                              
    if ($exists) {
        # Update existing series
        my $dbh = Bugzilla->dbh;
        $dbh->do("UPDATE series SET " .
                 "category = ?, subcategory = ?," .
                 "name = ?, frequency = ?, is_public = ?  " .
                 "WHERE series_id = ?", undef,
                 $category_id, $subcategory_id, $self->{'name'},
                 $self->{'frequency'}, $self->{'public'}, 
                 $self->{'series_id'});
    }
    else {
        # Insert the new series into the series table
        $dbh->do("INSERT INTO series (creator, category, subcategory, " .
                 "name, frequency, query, is_public) VALUES " . 
                 "(?, ?, ?, ?, ?, ?, ?)", undef,
                 $self->{'creator_id'}, $category_id, $subcategory_id, $self->{'name'},
                 $self->{'frequency'}, $self->{'query'}, $self->{'public'});

        # Retrieve series_id
        $self->{'series_id'} = $dbh->selectrow_array("SELECT MAX(series_id) " .
                                                     "FROM series");
        $self->{'series_id'}
          || ThrowCodeError("missing_series_id", { 'series' => $self });
    }
    
    $dbh->bz_commit_transaction();
}

# Check whether a series with this name, category and subcategory exists in
# the DB and, if so, returns its series_id.
sub existsInDatabase {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my $category_id = getCategoryID($self->{'category'});
    my $subcategory_id = getCategoryID($self->{'subcategory'});
    
    trick_taint($self->{'name'});
    my $series_id = $dbh->selectrow_array("SELECT series_id " .
                              "FROM series WHERE category = $category_id " .
                              "AND subcategory = $subcategory_id AND name = " .
                              $dbh->quote($self->{'name'}));
                              
    return($series_id);
}

# Get a category or subcategory IDs, creating the category if it doesn't exist.
sub getCategoryID {
    my ($category) = @_;
    my $category_id;
    my $dbh = Bugzilla->dbh;

    # This seems for the best idiom for "Do A. Then maybe do B and A again."
    while (1) {
        # We are quoting this to put it in the DB, so we can remove taint
        trick_taint($category);

        $category_id = $dbh->selectrow_array("SELECT id " .
                                      "from series_categories " .
                                      "WHERE name =" . $dbh->quote($category));

        last if defined($category_id);

        $dbh->do("INSERT INTO series_categories (name) " .
                 "VALUES (" . $dbh->quote($category) . ")");
    }

    return $category_id;
}

##########
# Methods
##########
sub id   { return $_[0]->{'series_id'}; }
sub name { return $_[0]->{'name'}; }

sub creator {
    my $self = shift;

    if (!$self->{creator} && $self->{creator_id}) {
        require Bugzilla::User;
        $self->{creator} = new Bugzilla::User($self->{creator_id});
    }
    return $self->{creator};
}

sub remove_from_db {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    $dbh->do('DELETE FROM series WHERE series_id = ?', undef, $self->id);
}

1;

=head1 B<Methods in need of POD>

=over

=item creator

=item existsInDatabase

=item name

=item getCategoryID

=item initFromParameters

=item initFromCGI

=item initFromDatabase

=item remove_from_db

=item writeToDatabase

=item id

=back
