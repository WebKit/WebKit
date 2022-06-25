#!/usr/bin/perl

print "Status: 302 Moved Temporarily\r\n";
print "Location: redirect-cycle-1.pl\r\n";
print "Content-type: text/html\r\n";
print "\r\n";
print "<html>";
print "<body>";
print "<div>Page 2</div>";
print "</body>";
print "</html>";
