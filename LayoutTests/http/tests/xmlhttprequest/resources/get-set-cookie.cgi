#!/usr/bin/perl -w

print "Content-type: text/plain\n"; 
print "Set-Cookie: WK-test=1\n";
print "Set-Cookie: WK-test-secure=1; secure\n\n";

if ($ENV{"HTTP_COOKIE"}) {
    print "$ENV{\"HTTP_COOKIE\"}\n";
}
