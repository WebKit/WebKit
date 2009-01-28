#!/usr/bin/perl
# Script to generate a 302 HTTP redirect

print "Status: 302 Moved Temporarily\r\n";
print "Location: resources/redirect-target.html?1\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<head>
<title>302 Redirect</title>
<script>
if (window.layoutTestController) {
    layoutTestController.keepWebHistory();
    layoutTestController.waitUntilDone();
}
</script>

<body>This page is a 302 redirect.</body>
</html>
HERE_DOC_END
