#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: sameorigin\n\n";

print <<"EOF";
<p>PASS: This should show up as all frame ancestors are same origin with this page.</p>
<script>
if (window.testRunner)
    testRunner.notifyDone();
</script>
EOF
