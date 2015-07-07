#!/usr/bin/perl
# Test that BOM can override whatever charset was set in Content-Type
# (this is not the behavior of Firefox, nor expected by any standard).

print "Content-type: text/javascript;charset=iso-8859-1\r\n";
print "\r\n";

print "\xef\xbb\xbf";
print "document.write('<p>SUССESS</p>');";
