#!/usr/bin/perl
# Simple script to generate a 302 HTTP redirect with http-equiv=refresh

print "Status: 302 Found\r\n";
print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<meta http-equiv="refresh" content="0;URL=redirect302-metaredirect.html">
<script>
    if (window.testRunner)
        testRunner.waitUntilDone();
</script>
<body>
Test failed! - This page uses a meta redirect to load another page.
The key aspect is that the browser should get the body of the 302
message and use it to perform a redirect
</body></html>
HERE_DOC_END
