#!/usr/bin/perl -w

print "Content-type: text/plain\n"; 
if ($ENV{"HTTP_COOKIE"}) {
    print "\n$ENV{\"HTTP_COOKIE\"}\n";
} else {
    print "Set-Cookie: WK-test=1\n";
    print "Set-Cookie: WK-test-secure=1; secure\n\n";
}
