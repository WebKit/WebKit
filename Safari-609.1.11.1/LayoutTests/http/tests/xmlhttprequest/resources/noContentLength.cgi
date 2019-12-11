#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n\n";

print <<EOF
<html>
<head>
<title>This page does not provide any content length</title>
</head>
<body>
<h1> Link to
<a href="http://webkit.org">WebKit's</a> site.
</h1>
</body>
</html>
EOF
