#!/usr/bin/perl -w

print "Content-type: text/plain\n";
my $AGE_STRING = "";
if ($ENV{"QUERY_STRING"}) { # Assume any query string means "?clear=1"
    $AGE_STRING = "expires=Thu, 19 Mar 1982 11:22:11 GMT";
}
print "Set-Cookie: WK-test=1;$AGE_STRING\n";
print "Set-Cookie: WK-test-secure=1; secure;$AGE_STRING\n";
print "Set-Cookie2: WK-test-2=1;$AGE_STRING\n\n";
print "$ENV{\"HTTP_COOKIE\"}\n" if $ENV{"HTTP_COOKIE"};
