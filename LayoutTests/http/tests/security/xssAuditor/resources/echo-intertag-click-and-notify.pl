#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<head>\n";
print "<script>\n";
print "window.onload = function()\n";
print "{\n";
print "    var event = document.createEvent('MouseEvent');\n";
print "    event.initEvent('click', true, true);\n";
print "    document.getElementById('".$cgi->param('elmid')."').dispatchEvent(event);\n";
print "}\n";
print "</script>\n";
print "</head>\n";
print "<body>\n";
print $cgi->param('q');
print "<script>\n";
print "if (window.layoutTestController)\n";
print "    layoutTestController.notifyDone();\n";
print "</script>\n";
print "</body>\n";
print "</html>\n";
