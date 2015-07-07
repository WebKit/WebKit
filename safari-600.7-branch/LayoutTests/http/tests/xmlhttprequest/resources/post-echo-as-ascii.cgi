#!/usr/bin/perl -w

print "Content-type: text/plain\n\n"; 

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'})
                || die "Could not get query\n";
    my @array = split //, $request;
    @array = map(ord, @array);
    print "@array";
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
} 
