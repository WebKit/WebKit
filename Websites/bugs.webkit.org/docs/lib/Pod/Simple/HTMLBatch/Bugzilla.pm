# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Pod::Simple::HTMLBatch::Bugzilla;

use 5.10.1;
use strict;
use warnings;

use parent qw(Pod::Simple::HTMLBatch);

# This is the same hack that HTMLBatch does to "import" this subroutine.
BEGIN { *esc = \&Pod::Simple::HTML::esc }

# Describes how top-level modules should be sorted and named. This
# is a translation from HTMLBatch's names to our categories.
# Note that if you leave out a category here, it will not be indexed
# in the contents file, even though its HTML POD will still exist.
use constant FILE_TRANSLATION => {
    Files      => ['importxml', 'contrib', 'checksetup', 'email_in', 
                   'install-module', 'sanitycheck', 'jobqueue', 'migrate',
                   'collectstats'],
    Modules    => ['bugzilla'],
    Extensions => ['extensions'],
};

# This is basically copied from Pod::Simple::HTMLBatch, and overridden
# so that we can format things more nicely.
sub _write_contents_middle {
    my ($self, $Contents, $outfile, $toplevel2submodules) = @_;

    my $file_trans = FILE_TRANSLATION;

    # For every top-level category...
    foreach my $category (sort keys %$file_trans) {
        # Get all of the HTMLBatch categories that should be in this
        # category.
        my @category_data;
        foreach my $b_category (@{$file_trans->{$category}}) {
            my $data = $toplevel2submodules->{$b_category};
            push(@category_data, @$data) if $data;
        }
        next unless @category_data;

        my @downlines = sort {$a->[-1] cmp $b->[-1]} @category_data;

        # And finally, actually print out the table for this category. 
        printf $Contents qq[<dt><a name="%s">%s</a></dt>\n<dd>\n],
                esc($category), esc($category);
        print $Contents '<table class="pod_desc_table">' . "\n";

        # For every POD...
        my $row_count = 0;
        foreach my $e (@downlines) {
            $row_count++;
            my $even_or_odd = $row_count % 2 ? 'even' : 'odd';
            my $name = esc($e->[0]);
            my $path = join( "/", '.', esc(@{$e->[3]}) )
               . $Pod::Simple::HTML::HTML_EXTENSION;
            my $description = $self->{bugzilla_desc}->{$name} || '';
            $description = esc($description);
            my $html = <<END_HTML;
<tr class="$even_or_odd">
  <th><a href="$path">$name</a></th>
  <td>$description</td>
</tr>
END_HTML
      
            print $Contents $html;
        }
        print $Contents "</table></dd>\n\n";
    }

    return 1;
}

# This stores the name and description for each file, so that
# we can get that information out later.
sub note_for_contents_file {
    my $self = shift;
    my $retval = $self->SUPER::note_for_contents_file(@_);

    my ($namelets, $infile) = @_;
    my $parser   = $self->html_render_class->new;
    $parser->set_source($infile);
    my $full_title = $parser->get_title;
    $full_title =~ /^\S+\s+-+\s+(.+)/;
    my $description = $1;
    
    $self->{bugzilla_desc} ||= {};
    $self->{bugzilla_desc}->{join('::', @$namelets)} = $description;

    return $retval;
}

# Exclude modules being in lib/.
sub find_all_pods {
    my($self, $dirs) = @_;
    my $mod2path = $self->SUPER::find_all_pods($dirs);
    foreach my $mod (keys %$mod2path) {
        delete $mod2path->{$mod} if $mod =~ /^lib::/;
    }
    return $mod2path;
}

1;
