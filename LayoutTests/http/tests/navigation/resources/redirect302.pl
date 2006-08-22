#!/usr/bin/perl
# Simple script to generate a 302 HTTP redirect

print "Status: 302 Moved Temporarily\r\n";
print "Location: success200.html\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html><body>
This page should not be seen - it is a 302 redirect to another page.
The key aspect is that this page should not wind up in the back/forward
list, since there should only be one entry for the navigation through
this page to the eventual target.
</body></html>
HERE_DOC_END
