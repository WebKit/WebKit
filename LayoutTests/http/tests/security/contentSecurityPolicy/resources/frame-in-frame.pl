#!/usr/bin/perl
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "    <script src='/security/contentSecurityPolicy/resources/frame-ancestors-test.js'></script>\n";
print "    <p>Testing a " . $cgi->param("child") . "-origin child with a policy of &quot;" . $cgi->param("policy") . "&quot; nested in a " . $cgi->param("parent") . "-origin parent.</p>";
print "    <script>\n";
print "        " . $cgi->param("child") . "OriginFrameShouldBe" . $cgi->param("expectation") . "(\"" . $cgi->param("policy") . "\");\n";
print "    </script>\n";
print "</body>\n";
print "</html>\n";
