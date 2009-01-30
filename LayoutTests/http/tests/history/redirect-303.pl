#!/usr/bin/perl
# Script to generate a 303 HTTP redirect

print "Status: 303 See Other\r\n";
print "Location: resources/redirect-target.html#1\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<head>
<title>303 Redirect</title>
<script>
if (window.layoutTestController) {
    layoutTestController.keepWebHistory();
    layoutTestController.waitUntilDone();
}
</script>

<body>This page is a 303 redirect.</body>
</html>
HERE_DOC_END
