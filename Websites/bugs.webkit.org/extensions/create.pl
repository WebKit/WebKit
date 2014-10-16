#!/usr/bin/env perl -w
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
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by the Initial Developer are Copyright (C) 2009 the
# Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use lib qw(. lib);
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util qw(get_text);

use File::Path qw(mkpath);
use DateTime;

my $base_dir = bz_locations()->{'extensionsdir'};

my $name = $ARGV[0] or ThrowUserError('extension_create_no_name');
if ($name !~ /^[A-Z]/) {
    ThrowUserError('extension_first_letter_caps', { name => $name });
}

my $extension_dir = "$base_dir/$name"; 
mkpath($extension_dir) 
  || die "$extension_dir already exists or cannot be created.\n";

my $lcname = lc($name);
foreach my $path (qw(lib web template/en/default/hook), 
                  "template/en/default/$lcname")
{
    mkpath("$extension_dir/$path") || die "$extension_dir/$path: $!";
}

my $year = DateTime->now()->year;

my $template = Bugzilla->template;
my $vars = { year => $year, name => $name, path => $extension_dir };
my %create_files = (
    'config.pm.tmpl'       => 'Config.pm',
    'extension.pm.tmpl'    => 'Extension.pm',
    'util.pm.tmpl'         => 'lib/Util.pm',
    'web-readme.txt.tmpl'  => 'web/README',
    'hook-readme.txt.tmpl' => 'template/en/default/hook/README',
    'name-readme.txt.tmpl' => "template/en/default/$lcname/README",
);

foreach my $template_file (keys %create_files) {
    my $target = $create_files{$template_file};
    my $output;
    $template->process("extensions/$template_file", $vars, \$output)
      or ThrowTemplateError($template->error());
   open(my $fh, '>', "$extension_dir/$target");
   print $fh $output;
   close($fh);
}

print get_text('extension_created', $vars), "\n";

__END__

=head1 NAME

extensions/create.pl - Create a framework for a new Bugzilla Extension.

=head1 SYNOPSIS

 extensions/create.pl NAME

 Creates a framework for an extension called NAME in the F<extensions/>
 directory.
