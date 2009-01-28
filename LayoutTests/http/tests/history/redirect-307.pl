#!/usr/bin/perl
# Script to generate a 307 HTTP redirect

print "Status: 307 Moved Temporarily\r\n";
print "Location: resources/redirect-target.html?1\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<head>
<title>307 Redirect</title>
<script>
if (window.layoutTestController) {
    layoutTestController.keepWebHistory();
    layoutTestController.waitUntilDone();
}
</script>
</head>

<body>This page is a 307 redirect.</body>
</html>
HERE_DOC_END
