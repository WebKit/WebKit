#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Origin: *\n";
print "Cache-Control: no-store\n\n";

print <<DONE
<!DOCTYPE html>
<html>
<head>
    <script src="/js-test-resources/testharness.js"></script>
    <script src="/js-test-resources/testharnessreport.js"></script>
</head>
<body>
    <p>Verify that this request was delivered with an 'Upgrade-Insecure-Requests' header.</p>
    <script>
    var httpsHeader = "$ENV{"HTTP_UPGRADE_INSECURE_REQUESTS"}";
    document.write("Upgrade-Insecure-Requests: " + httpsHeader)
    assert_equals(httpsHeader, "1");
    if (window.testRunner)
        testRunner.notifyDone();
    </script>
</body>
</html>
DONE
