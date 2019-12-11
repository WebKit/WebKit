#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=" . ($cgi->param("charset") || "UTF8") . "\n";
if ($cgi->param("header")) {
    print $cgi->param("header") . "\n";
}
print "\n\n";
print <<EOF;
<!DOCTYPE html>
<html>
<head>
EOF
if ($cgi->param("head")) {
    print $cgi->param("head") . "\n";
}
print <<EOF;
</head>
<body>
EOF
print $cgi->param("q") . "\n";
if ($cgi->param("notifyDidLoad")) {
    print '<script nonce="notifyDidLoad">window.top.postMessage("dispatchDidLoad", "*")</script>' . "\n";
}
print <<EOF;
</body>
</html>
EOF
