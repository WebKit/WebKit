# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Report;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::CGI;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

use constant DB_TABLE => 'reports';

# Do not track reports saved by users.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;
use constant AUDIT_REMOVES => 0;

use constant DB_COLUMNS => qw(
    id
    user_id
    name
    query
);

use constant UPDATE_COLUMNS => qw(
    name
    query
);

use constant VALIDATORS => {
    name    => \&_check_name,
    query   => \&_check_query,
};

##############
# Validators #
##############

sub _check_name {
    my ($invocant, $name) = @_;
    $name = clean_text($name);
    $name || ThrowUserError("report_name_missing");
    $name !~ /[<>&]/ || ThrowUserError("illegal_query_name");
    if (length($name) > MAX_LEN_QUERY_NAME) {
        ThrowUserError("query_name_too_long");
    }
    return $name;
}

sub _check_query {
    my ($invocant, $query) = @_;
    $query || ThrowUserError("buglist_parameters_required");
    my $cgi = new Bugzilla::CGI($query);
    $cgi->clean_search_url;
    return $cgi->query_string;
}

#############
# Accessors #
#############

sub query { return $_[0]->{'query'}; }

sub set_name { $_[0]->set('name', $_[1]); }
sub set_query { $_[0]->set('query', $_[1]); }

###########
# Methods #
###########

sub create {
    my $class = shift;
    my $param = shift;

    Bugzilla->login(LOGIN_REQUIRED);
    $param->{'user_id'} = Bugzilla->user->id;

    unshift @_, $param;
    my $self = $class->SUPER::create(@_);
}

sub check {
    my $class = shift;
    my $report = $class->SUPER::check(@_);
    my $user = Bugzilla->user;
    if ( grep($_->id eq $report->id, @{$user->reports})) {
        return $report;
    } else {
        ThrowUserError('report_access_denied');
    }
}

1;

__END__

=head1 NAME

Bugzilla::Report - Bugzilla report class.

=head1 SYNOPSIS

    use Bugzilla::Report;

    my $report = new Bugzilla::Report(1);

    my $report = Bugzilla::Report->check({id => $id});

    my $name = $report->name;
    my $query = $report->query;

    my $report = Bugzilla::Report->create({ name => $name, query => $query });

    $report->set_name($new_name);
    $report->set_query($new_query);
    $report->update();

    $report->remove_from_db;

=head1 DESCRIPTION

Report.pm represents a Report object. It is an implementation
of L<Bugzilla::Object>, and thus provides all methods that
L<Bugzilla::Object> provides.

=cut

=head1 B<Methods in need of POD>

=over

=item create

=item query

=item set_query

=item set_name

=back
