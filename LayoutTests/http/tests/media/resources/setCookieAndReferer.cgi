#!/usr/bin/perl -wT
use CGI;

$query = new CGI;
$name = $query->param('name');
$referer = $query->param('referer');

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

print "Referer: ${referer}\n";

# We only map the SET_COOKIE request header to "Set-Cookie"
print "Set-Cookie: TEST=${name}\n\n";
