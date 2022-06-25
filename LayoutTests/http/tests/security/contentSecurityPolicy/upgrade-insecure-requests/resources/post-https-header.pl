#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Origin: *\n";
print "Cache-Control: no-store\n\n";

print <<DONE
<script>
    var target = window.opener || window.top;
    target.postMessage({ "header": "$ENV{"HTTP_UPGRADE_INSECURE_REQUESTS"}" }, "*");
</script>
DONE
