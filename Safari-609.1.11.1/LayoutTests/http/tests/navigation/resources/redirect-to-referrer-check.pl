#!/usr/bin/perl
# Simple script to generate a 302 HTTP redirect

print "Status: 302 Moved Temporarily\r\n";
print "Location: referrer.html\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html><body>
This page should not be seen - it is a 302 redirect to another page.
</body></html>
HERE_DOC_END
