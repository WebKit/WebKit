#!/usr/bin/perl

if ($ENV{HTTP_CSP} eq "active") {
    print "Status: 200\r\n";
    print "Content-type: application/javascript\r\n";
    print "\r\n";
    print "script_loaded = true;\r\n\r\n"
} else {
    print "Status: 404\r\n";
    print "Content-type: text/html\r\n";
    print "\r\n";
    print "CSP header not sent\r\n\r\n";
}
