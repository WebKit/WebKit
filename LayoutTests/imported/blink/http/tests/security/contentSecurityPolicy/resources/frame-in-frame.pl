#!/usr/bin/perl
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "    <script src='/js-test-resources/js-test.js'></script>\n";
print "    <script src='/security/contentSecurityPolicy/resources/frame-ancestors-test.js'></script>\n";
print "    <script>\n";
print "        description(\"Testing a " . $cgi->param("child") . "-origin child with a policy of \\\"" . $cgi->param("policy") . "\\\" nested in a " . $cgi->param("parent") . "-origin parent.\");\n";
print "        " . $cgi->param("child") . "OriginFrameShouldBe" . $cgi->param("expectation") . "(\"" . $cgi->param("policy") . "\");\n";
print "    </script>\n";
print "</body>\n";
print "</html>\n";
