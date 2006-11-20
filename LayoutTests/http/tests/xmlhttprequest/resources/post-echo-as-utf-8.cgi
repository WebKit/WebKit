#!/usr/bin/perl -w
#use CGI ();

print "Content-type: text/plain;charset=utf-8\n\n";

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'})
                || die "Could not get query\n";
#    print CGI::escape($request);
     print $request;
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
} 
