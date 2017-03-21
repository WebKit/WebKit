#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

###############################################################################
# This CGI is a general template display engine. To display templates using it,
# put them in the "pages" subdirectory of en/default, call them
# "foo.<ctype>.tmpl" and use the URL page.cgi?id=foo.<ctype> , where <ctype> is
# a content-type, e.g. html.
###############################################################################

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Error;
use Bugzilla::Hook;
use Bugzilla::Search::Quicksearch;

###############
# Subroutines #
###############

# For quicksearch.html.
sub quicksearch_field_names {
    my $fields = Bugzilla::Search::Quicksearch->FIELD_MAP;
    my %fields_reverse;
    # Put longer names before shorter names.
    my @nicknames = sort { length($b) <=> length($a) } (keys %$fields);
    foreach my $nickname (@nicknames) {
        my $db_field = $fields->{$nickname};
        $fields_reverse{$db_field} ||= [];
        push(@{ $fields_reverse{$db_field} }, $nickname);
    }
    return \%fields_reverse;
}

###############
# Main Script #
###############

Bugzilla->login();

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

my $id = $cgi->param('id');
if ($id) {
    # Be careful not to allow directory traversal.
    if ($id =~ /\.\./) {
        # two dots in a row is bad
        ThrowUserError("bad_page_cgi_id", { "page_id" => $id });
    }
    # Split into name and ctype.
    $id =~ /^([\w\-\/\.]+)\.(\w+)$/;
    if (!$2) {
        # if this regexp fails to match completely, something bad came in
        ThrowUserError("bad_page_cgi_id", { "page_id" => $id });
    }

    my %vars = ( 
      quicksearch_field_names => \&quicksearch_field_names,
    );
    Bugzilla::Hook::process('page_before_template', 
                            { page_id => $id, vars => \%vars });

    my $format = $template->get_format("pages/$1", undef, $2);
    
    $cgi->param('id', $id);

    print $cgi->header($format->{'ctype'});

    $template->process("$format->{'template'}", \%vars)
      || ThrowTemplateError($template->error());
}
else {
    ThrowUserError("no_page_specified");
}
