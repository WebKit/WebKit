#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This script compiles all the documentation.
#
# Required software:
#
# 1) Sphinx documentation builder (python-sphinx package on Debian/Ubuntu)
#
# 2a) rst2pdf
# or
# 2b) pdflatex, which means the following Debian/Ubuntu packages:
#     * texlive-latex-base
#     * texlive-latex-recommended
#     * texlive-latex-extra
#     * texlive-fonts-recommended
#
# All these TeX packages together are close to a gig :-| But after you've
# installed them, you can remove texlive-latex-extra-doc to save 400MB.

use 5.10.1;
use strict;
use warnings;

use File::Basename;
BEGIN { chdir dirname($0); }

use lib qw(.. ../lib lib);

use Cwd;
use File::Copy::Recursive qw(rcopy);
use File::Find;
use File::Path qw(rmtree make_path);
use File::Which qw(which);
use Pod::Simple;

use Bugzilla::Constants qw(BUGZILLA_VERSION bz_locations);
use Pod::Simple::HTMLBatch::Bugzilla;
use Pod::Simple::HTML::Bugzilla;

###############################################################################
# Subs
###############################################################################

my $error_found = 0;
sub MakeDocs {
    my ($name, $cmdline) = @_;

    say "Creating $name documentation ..." if defined $name;
    system('make', $cmdline) == 0
        or $error_found = 1;
    print "\n";
}

sub make_pod {
    say "Creating API documentation...";

    my $converter = Pod::Simple::HTMLBatch::Bugzilla->new;
    # Don't output progress information.
    $converter->verbose(0);
    $converter->html_render_class('Pod::Simple::HTML::Bugzilla');

    my $doctype      = Pod::Simple::HTML::Bugzilla->DOCTYPE;
    my $content_type = Pod::Simple::HTML::Bugzilla->META_CT;
    my $bz_version   = BUGZILLA_VERSION;

    my $contents_start = <<END_HTML;
$doctype
<html>
  <head>
    $content_type
    <title>Bugzilla $bz_version API Documentation</title>
  </head>
  <body class="contentspage">
    <h1>Bugzilla $bz_version API Documentation</h1>
END_HTML

    $converter->contents_page_start($contents_start);
    $converter->contents_page_end("</body></html>");
    $converter->add_css('./../../../../style.css');
    $converter->javascript_flurry(0);
    $converter->css_flurry(0);
    make_path('html/integrating/api');
    $converter->batch_convert(['../../'], 'html/integrating/api');

    print "\n";
}

###############################################################################
# Make the docs ...
###############################################################################

my @langs;
# search for sub directories which have a 'rst' sub-directory
opendir(LANGS, './');
foreach my $dir (readdir(LANGS)) {
    next if (($dir eq '.') || ($dir eq '..') || (! -d $dir));
    if (-d "$dir/rst") {
        push(@langs, $dir);
    }
}
closedir(LANGS);

my $docparent = getcwd();
foreach my $lang (@langs) {
    chdir "$docparent/$lang";

    make_pod();

    next if grep { $_ eq '--pod-only' } @ARGV;

    chdir $docparent;

    # Generate extension documentation, both normal and API
    my $ext_dir = bz_locations()->{'extensionsdir'};
    my @ext_paths = grep { $_ !~ /\/create\.pl$/ && ! -e "$_/disabled" }
                    glob("$ext_dir/*");
    my %extensions;
    foreach my $item (@ext_paths) {
        my $basename = basename($item);
        if (-d "$item/docs/$lang/rst") {
            $extensions{$basename} = "$item/docs/$lang/rst";
        }
    }

    # Collect up local extension documentation into the extensions/ dir.
    rmtree("$lang/rst/extensions", 0, 1);

    foreach my $ext_name (keys %extensions) {
        my $src = $extensions{$ext_name} . "/*";
        my $dst = "$docparent/$lang/rst/extensions/$ext_name";
        mkdir($dst) unless -d $dst;
        rcopy($src, $dst);
    }

    chdir "$docparent/$lang";

    MakeDocs('HTML', 'html');
    MakeDocs('TXT', 'text');

    if (grep { $_ eq '--with-pdf' } @ARGV) {
        if (which('pdflatex')) {
            MakeDocs('PDF', 'latexpdf');
        }
        elsif (which('rst2pdf')) {
            rmtree('pdf', 0, 1);
            MakeDocs('PDF', 'pdf');
        }
        else {
            say 'pdflatex or rst2pdf not found. Skipping PDF file creation';
        }
    }

    rmtree('doctrees', 0, 1);
}

die "Error occurred building the documentation\n" if $error_found;
