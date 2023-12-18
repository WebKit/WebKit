#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Origin: *\n";
print "Cache-Control: no-store\n\n";

print "HTTP_ORIGIN: " . $ENV{"HTTP_ORIGIN"} . "\n";
print <<DONE
<script>
    if (window.testRunner)
        window.testRunner.notifyDone();
</script>
DONE
