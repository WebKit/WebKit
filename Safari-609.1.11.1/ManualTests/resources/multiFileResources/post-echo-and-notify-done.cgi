#!/usr/bin/perl -w

print "Content-type: text/html\n\n"; 

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'})
                || die "Could not get query\n";
    print "<pre>$request</pre>";
    print "<script>if (window.testRunner) testRunner.notifyDone();</script>";
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
} 
