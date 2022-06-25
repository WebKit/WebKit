#!/usr/bin/perl -w

use CGI;
use File::stat;
use Time::HiRes;

$query = new CGI;
$name = $query->param('name');
$waitFor = $query->param('waitFor');
$mimeType = $query->param('mimeType');

my $filesize = stat($name)->size;
print "Content-type: " . $mimeType . "\n"; 
print "Content-Length: " . $filesize . "\n\n";

open FILE, $name or die;
binmode FILE;
$total = 0;
my ($buf, $data, $n);
while (($n = read FILE, $data, 1024) != 0) {
    $total += $n;
    print $data;
}
close(FILE);
if (defined $waitFor) {
    Time::HiRes::sleep($waitFor)
}

