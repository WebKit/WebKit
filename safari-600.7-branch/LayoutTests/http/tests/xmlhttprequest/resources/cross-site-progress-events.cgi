#!/usr/bin/perl -w

print "Content-type: text/plain\n"; 

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    if ($ENV{'QUERY_STRING'} eq "allow") {
        print "Access-Control-Allow-Origin: *\n";
    }
    print "\n";
    if ($ENV{'CONTENT_LENGTH'}) {
        read(STDIN, $request, $ENV{'CONTENT_LENGTH'}) || die "Could not get query\n";
    } else {
        $request = "";
    }
    print $request;
} else {
    print "\n";
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
} 
