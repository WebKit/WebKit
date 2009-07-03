#!/usr/bin/perl -w
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
# Contributor(s): Matthew Tuck <matty@chariot.net.au>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Colin Ogilvie <colin.ogilvie@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

# This script compiles all the documentation.

use strict;

# We need to be in this directory to use our libraries.
BEGIN {
    require File::Basename;
    import File::Basename qw(dirname);
    chdir dirname($0);
}

use lib qw(.. lib);

# We only compile our POD if Pod::Simple is installed. We do the checks
# this way so that if there's a compile error in Pod::Simple::HTML::Bugzilla,
# makedocs doesn't just silently fail, but instead actually tells us there's
# a compile error.
my $pod_simple;
if (eval { require Pod::Simple }) {
    require Pod::Simple::HTMLBatch::Bugzilla;
    require Pod::Simple::HTML::Bugzilla;
    $pod_simple = 1;
};

use Bugzilla::Install::Requirements 
    qw(REQUIRED_MODULES OPTIONAL_MODULES);
use Bugzilla::Constants qw(DB_MODULE BUGZILLA_VERSION);

###############################################################################
# Generate minimum version list
###############################################################################

my $modules = REQUIRED_MODULES;
my $opt_modules = OPTIONAL_MODULES;

open(ENTITIES, '>', 'xml/bugzilla.ent') or die('Could not open xml/bugzilla.ent: ' . $!);
print ENTITIES '<?xml version="1.0"?>' ."\n\n";
print ENTITIES '<!-- Module Versions -->' . "\n";
foreach my $module (@$modules, @$opt_modules)
{
    my $name = $module->{'module'};
    $name =~ s/::/-/g;
    $name = lc($name);
    #This needs to be a string comparison, due to the modules having
    #version numbers like 0.9.4
    my $version = $module->{'version'} eq 0 ? 'any' : $module->{'version'};
    print ENTITIES '<!ENTITY min-' . $name . '-ver "'.$version.'">' . "\n";
}

# CGI is a special case, because it has an optional version *and* a required
# version.
my ($cgi_opt) = grep($_->{package} eq 'CGI', @$opt_modules);
print ENTITIES '<!ENTITY min-mp-cgi-ver "' . $cgi_opt->{version} . '">' . "\n";

print ENTITIES "\n <!-- Database Versions --> \n";

my $db_modules = DB_MODULE;
foreach my $db (keys %$db_modules) {
    my $dbd  = $db_modules->{$db}->{dbd};
    my $name = $dbd->{package};
    $name = lc($name);
    my $version    = $dbd->{version} || 'any';
    my $db_version = $db_modules->{$db}->{'db_version'};
    print ENTITIES '<!ENTITY min-' . $name . '-ver "'.$version.'">' . "\n";
    print ENTITIES '<!ENTITY min-' . lc($db) . '-ver "'.$db_version.'">' . "\n";
}
close(ENTITIES);

###############################################################################
# Environment Variable Checking
###############################################################################

my ($JADE_PUB, $LDP_HOME, $build_docs);
$build_docs = 1;
if (defined $ENV{JADE_PUB} && $ENV{JADE_PUB} ne '') {
    $JADE_PUB = $ENV{JADE_PUB};
}
else {
    print "To build 'The Bugzilla Guide', you need to set the ";
    print "JADE_PUB environment variable first.\n";
    $build_docs = 0;
}

if (defined $ENV{LDP_HOME} && $ENV{LDP_HOME} ne '') {
    $LDP_HOME = $ENV{LDP_HOME};
}
else {
    print "To build 'The Bugzilla Guide', you need to set the ";
    print "LDP_HOME environment variable first.\n";
    $build_docs = 0;
}

###############################################################################
# Subs
###############################################################################

sub MakeDocs {

    my ($name, $cmdline) = @_;

    print "Creating $name documentation ...\n" if defined $name;
    print "$cmdline\n\n";
    system $cmdline;
    print "\n";

}

sub make_pod {

    print "Creating API documentation...\n";

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
    $converter->add_css('style.css');
    $converter->javascript_flurry(0);
    $converter->css_flurry(0);
    $converter->batch_convert(['../'], 'html/api/');

    print "\n";
}

###############################################################################
# Make the docs ...
###############################################################################

if (!-d 'txt') {
    unlink 'txt';
    mkdir 'txt', 0755;
}
if (!-d 'pdf') {
    unlink 'pdf';
    mkdir 'pdf', 0755;
}

make_pod() if $pod_simple;
exit unless $build_docs;

chdir 'html';

MakeDocs('separate HTML', "jade -t sgml -i html -d $LDP_HOME/ldp.dsl\#html " .
	 "$JADE_PUB/xml.dcl ../xml/Bugzilla-Guide.xml");
MakeDocs('big HTML', "jade -V nochunks -t sgml -i html -d " .
         "$LDP_HOME/ldp.dsl\#html $JADE_PUB/xml.dcl " .
	 "../xml/Bugzilla-Guide.xml > Bugzilla-Guide.html");
MakeDocs('big text', "lynx -dump -justify=off -nolist Bugzilla-Guide.html " .
	 "> ../txt/Bugzilla-Guide.txt");

if (! grep($_ eq "--with-pdf", @ARGV)) {
    exit;
}

MakeDocs('PDF', "jade -t tex -d $LDP_HOME/ldp.dsl\#print $JADE_PUB/xml.dcl " .
         '../xml/Bugzilla-Guide.xml');
chdir '../pdf';
MakeDocs(undef, 'mv ../xml/Bugzilla-Guide.tex .');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'rm Bugzilla-Guide.tex Bugzilla-Guide.log Bugzilla-Guide.aux Bugzilla-Guide.out');

