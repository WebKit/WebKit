#!/usr/bin/perl -w

print "Content-type: text/plain\n"; 
print "Set-Cookie: WK-test=1\n";
print "Set-Cookie: WK-test-secure=1; secure\n\n";
print "Set-Cookie2: WK-test-2=1\n";
print "$ENV{\"HTTP_COOKIE\"}\n" if $ENV{"HTTP_COOKIE"};
