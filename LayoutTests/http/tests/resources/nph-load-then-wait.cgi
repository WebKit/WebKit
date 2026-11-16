#!/usr/bin/perl -w

use CGI;
use File::stat;
use Time::HiRes;

$| = 1;

my $query = CGI->new;
my $name = $query->param('name');
my $waitFor = $query->param('waitFor');
my $mimeType = $query->param('mimeType');

my $filesize = stat($name)->size;
print "HTTP/1.1 200 OK\r\n";
print "Content-Type: $mimeType\r\n";
print "Content-Length: $filesize\r\n";
print "Connection: close\r\n";
print "\r\n";

open(my $fh, '<', $name) or die;
binmode $fh;
my $data;
while (read($fh, $data, 1024)) {
    print $data;
}
close($fh);

Time::HiRes::sleep($waitFor) if defined $waitFor;
