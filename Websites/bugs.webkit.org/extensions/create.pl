#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use File::Basename;
BEGIN { chdir dirname($0); }

use lib qw(.. ../lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util qw(get_text);

use File::Path qw(mkpath);

my $base_dir = bz_locations()->{'extensionsdir'};

my $name = $ARGV[0] or ThrowUserError('extension_create_no_name');
$name = ucfirst($name);
if ($name !~ /^[A-Z]/) {
    ThrowUserError('extension_first_letter_caps', { name => $name });
}

my $extension_dir = "$base_dir/$name"; 
mkpath($extension_dir) 
  || die "$extension_dir already exists or cannot be created.\n";

my $lcname = lc($name);
foreach my $path (qw(lib docs/en/rst web template/en/default/hook),
                  "template/en/default/$lcname")
{
    mkpath("$extension_dir/$path") || die "$extension_dir/$path: $!";
}

my $template = Bugzilla->template;
my $vars = { name => $name, path => $extension_dir };
my %create_files = (
    'config.pm.tmpl'       => 'Config.pm',
    'extension.pm.tmpl'    => 'Extension.pm',
    'util.pm.tmpl'         => 'lib/Util.pm',
    'web-readme.txt.tmpl'  => 'web/README',
    'hook-readme.txt.tmpl' => 'template/en/default/hook/README',
    'name-readme.txt.tmpl' => "template/en/default/$lcname/README",
    'index-admin.rst.tmpl' => "docs/en/rst/index-admin.rst",
    'index-user.rst.tmpl'  => "docs/en/rst/index-user.rst",
);

foreach my $template_file (keys %create_files) {
    my $target = $create_files{$template_file};
    my $output;
    $template->process("extensions/$template_file", $vars, \$output)
      or ThrowTemplateError($template->error());
    open(my $fh, '>', "$extension_dir/$target")
      or die "extension_dir/$target: $!";
    print $fh $output;
    close($fh);
}

say get_text('extension_created', $vars);

__END__

=head1 NAME

extensions/create.pl - Create a framework for a new Bugzilla Extension.

=head1 SYNOPSIS

 extensions/create.pl NAME

 Creates a framework for an extension called NAME in the F<extensions/>
 directory.
