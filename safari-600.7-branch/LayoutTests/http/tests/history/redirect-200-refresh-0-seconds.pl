#!/usr/bin/perl
# Script to generate a Refresh header HTTP redirect

print "Status: 200 ok\r\n";
print "Refresh: 0; url=resources/redirect-target.html#1\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<head>
<title>200 Refresh Redirect</title>
<script>
if (window.testRunner) {
    testRunner.clearBackForwardList();
    testRunner.waitUntilDone();
}
</script>

<body>This page is a 200 refresh redirect on a 0 second delay.</body>
</html>
HERE_DOC_END
