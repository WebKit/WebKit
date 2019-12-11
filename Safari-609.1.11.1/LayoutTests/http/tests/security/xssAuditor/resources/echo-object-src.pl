#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
if ($cgi->param('relay-target-ids-for-event')) {
    print "<script>\n";
    print "document.addEventListener('" . $cgi->param('relay-target-ids-for-event') . "', function(event) {\n";
    print "    window.parent.postMessage(event.target.id, '*');\n";
    print "}, true);\n";
    print "</script>\n";
}
print "<body>\n";
print "<object id=\"object\" name=\"plugin\" type=\"application/x-webkit-test-netscape\">\n";
print "<param name=\"movie\" value=\"".$cgi->param('q')."\" />\n";
print "</object>\n";
print "</body>\n";
print "</html>\n";
