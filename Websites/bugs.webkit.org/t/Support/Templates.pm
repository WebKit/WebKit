# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Support::Templates;

use 5.10.1;
use strict;
use warnings;

use lib 't';
use parent qw(Exporter);
@Support::Templates::EXPORT =
    qw(@languages @include_paths @english_default_include_paths
       @referenced_files %actual_files $num_actual_files);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Install::Util qw(template_include_path);
use Support::Files;

use File::Find;
use File::Spec;

# English default include paths
our @english_default_include_paths =
    (File::Spec->catdir(bz_locations()->{'templatedir'}, 'en', 'default'));

# And the extensions too
foreach my $extension (@Support::Files::extensions) {
    my $dir = File::Spec->catdir($extension, 'template', 'en', 'default');
    if (-e $dir) {
        push @english_default_include_paths, $dir;
    }
}

# Files which are referenced in the cgi files
our @referenced_files = ();

# All files sorted by include_path
our %actual_files = ();

# total number of actual_files
our $num_actual_files = 0;

# Set the template available languages and include paths
our @languages = @{ Bugzilla->languages };
our @include_paths = @{ template_include_path({ language => Bugzilla->languages }) };

our @files;

# Local subroutine used with File::Find
sub find_templates {
    # Prune CVS directories
    if (-d $_ && $_ eq 'CVS') {
        $File::Find::prune = 1;
        return;
    }

    # Only include files ending in '.tmpl'
    if (-f $_ && $_ =~ m/\.tmpl$/i) {
        my $filename;
        my $local_dir = File::Spec->abs2rel($File::Find::dir,
                                            $File::Find::topdir);

        # File::Spec 3.13 and newer return "." instead of "" if both
        # arguments of abs2rel() are identical.
        $local_dir = "" if ($local_dir eq ".");

        if ($local_dir) {
            $filename = File::Spec->catfile($local_dir, $_);
        } else {
            $filename = $_;
        }

        push(@files, $filename);
    }
}

# Scan the given template include path for templates
sub find_actual_files {
  my $include_path = $_[0];
  @files = ();
  find(\&find_templates, $include_path);
  return @files;
}


foreach my $include_path (@include_paths) {
  $actual_files{$include_path} = [ find_actual_files($include_path) ];
  $num_actual_files += scalar(@{$actual_files{$include_path}});
}

# Scan Bugzilla's perl code looking for templates used and put them
# in the @referenced_files array to be used by the 004template.t test.
my %seen;

foreach my $file (@Support::Files::testitems) {
    open (FILE, $file);
    my @lines = <FILE>;
    close (FILE);
    foreach my $line (@lines) {
        if ($line =~ m/template->process\(\"(.+?)\", .+?\)/) {
            my $template = $1;
            # Ignore templates with $ in the name, since they're
            # probably vars, not real files
            next if $template =~ m/\$/;
            next if $seen{$template};
            push (@referenced_files, $template);
            $seen{$template} = 1;
        }
    }
}

1;
