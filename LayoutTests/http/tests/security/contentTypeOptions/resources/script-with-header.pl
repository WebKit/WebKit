#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

if ($cgi->param('mime')) {
    print "Content-Type: " . $cgi->param('mime') . "\n";
}
if ($cgi->param('options')) {
    print "X-Content-Type-Options: " . $cgi->param('options') . "\n";
} else {
    print "X-Content-Type-Options: nosniff\n";
}
print "\n";
print "console.log(\"Executed script with MIME type: '" .  $cgi->param('mime') . "'.\");\n";
print "window.scriptsSuccessfullyLoaded++;\n";
