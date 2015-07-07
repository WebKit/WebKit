#!/usr/bin/perl

print "Content-type: text/plain\r\n";
print "\r\n";

@keypairs = split(/&/, $ENV{'QUERY_STRING'});

$text = "";

foreach $pair (@keypairs)
{
    ($name, $value) = split(/=/, $pair);

     if ($name eq "text") {
         $text = $value;
     }
}

print $text;
