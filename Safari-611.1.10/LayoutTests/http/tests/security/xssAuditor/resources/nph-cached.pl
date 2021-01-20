#!/usr/bin/perl
# Script to generate a 304 HTTP status with (illegal) X-XSS-Protect headers.
# Relies on its nph- filename to invoke the CGI non-parsed-header facility.

$protocol = $ENV{'SERVER_PROTOCOL'};
$software = $ENV{'SERVER_SOFTWARE'};
$if_modified = $ENV{'HTTP_IF_MODIFIED_SINCE'};
$nonce = int(rand(100000));

if ($if_modified) {
    print "$protocol 304 Not Modified\r\n";
    print "Server: $software\r\n";
    print "X-XSS-Protection: 0\r\n";
    print "Connection: close\r\n";
    exit(0);
}

print "$protocol 200 OK\r\n";
print "Server: $software\r\n";
print "Content-type: text/html\r\n";
print "Expires: Wed, 21 Nov 2037 19:19:13 +0000\r\n";
print "Last-Modified: Mon, 08 Nov 2010 19:19:13 +0000\r\n";
print "X-XSS-Protection: 1\r\n";
print "\r\n";
print "<!DOCTYPE html>\r\n";
print "<html>\r\n";
print "<body>\r\n";
print "<input id=\"rand\" type=\"text\" value=\"$nonce\"/>\r\n";
print "<script>alert(/XSS/);</script>\r\n";
print "</body>\r\n";
print "</html>\r\n";
