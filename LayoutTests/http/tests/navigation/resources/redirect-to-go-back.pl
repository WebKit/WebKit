#!/usr/bin/perl
# Simple script to generate a 302 HTTP redirect to a page that triggers a 
# navigation back (only if POST is used).

use CGI;
$query = new CGI;

$method = $query->request_method();

if ($method eq "POST") {
    print "Status: 302 Moved Temporarily\r\n";
    print "Location: top-go-back.html\r\n";
    print "\r\n";
} else {
    print "Status: 405 Method not allowed\r\n";
    print "Content-type: text/plain\r\n";
    print "\r\n";
    print "This should only be requested via POST ($method was used)."
}
