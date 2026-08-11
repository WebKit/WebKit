#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: sameorigin\n\n";

print "<p>PASS: This should show up as the parent is in the same origin.</p>\n";

if ($cgi->param("notifyDone")) {
    print "<script>\n";
    print "if (window.testRunner)\n";
    print "    testRunner.notifyDone();\n";
    print "</script>\n";
}
