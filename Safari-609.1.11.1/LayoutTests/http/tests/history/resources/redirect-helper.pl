#!/usr/bin/perl
# Script to generate a 30x HTTP redirect (determined by the query parameter)

$REDIRECT_CODE = $ENV{'QUERY_STRING'} || '301';

$STATUS_TEXTS = {
  '301' => 'Moved Permanently',
  '302' => 'Moved Temporarily',
  '303' => 'See Other',
  '307' => 'Moved Temporarily'
};

print "Status: $REDIRECT_CODE $STATUS_TEXTS{$REDIRECT_CODE}\r\n";
print "Location: redirect-target.html#2\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<head>
<title>$REDIRECT_CODE Redirect</title>

<body>This page is a $REDIRECT_CODE redirect.</body>
</html>
HERE_DOC_END
