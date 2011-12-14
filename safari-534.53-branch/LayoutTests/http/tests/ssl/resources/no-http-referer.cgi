#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-store\n\n";

my $failed = 0;
foreach (keys %ENV) {
    if ($_ =~ "HTTP_REFERER") {
        print "FAIL. " . $_ . ": " . $ENV{$_} . "\n";
        $failed = 1;
    }
}
if (!$failed) {
    print "PASS\n";
}

print "<script>if (window.layoutTestController) layoutTestController.notifyDone()</script>";
