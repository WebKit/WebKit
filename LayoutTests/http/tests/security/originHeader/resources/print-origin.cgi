#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Origin: *\n";
print "Cache-Control: no-store\n\n";

print "HTTP_ORIGIN: " . $ENV{"HTTP_ORIGIN"} . "\n";
print <<DONE
<script>
    if (window.layoutTestController)
        window.layoutTestController.notifyDone();
</script>
DONE
