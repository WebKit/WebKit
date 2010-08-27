#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

if ($cgi->param('enable-full-block')) {
    print "X-XSS-Protection: 1; mode=block\n";
}
if ($cgi->param('disable-protection')) {
    print "X-XSS-Protection: 0\n";
}
if ($cgi->param('crazy-header')) {
    print "X-XSS-Protection:   1  ;MoDe =  bLocK   \n";
}
if ($cgi->param('custom-header')) {
    print $cgi->param('custom-header') . "\n";
}
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
print $cgi->param('q');
if ($cgi->param('notifyDone')) {
    print "<script>\n";
    print "if (window.layoutTestController)\n";
    print "    layoutTestController.notifyDone();\n";
    print "</script>\n";
}
if ($cgi->param('enable-full-block')) {
    print "<p>If you see this message then the test FAILED.</p>\n";
}
print "</body>\n";
print "</html>\n";
