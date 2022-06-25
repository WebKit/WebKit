#!/usr/bin/perl -w

use CGI;
use File::stat;
use Time::HiRes;

$query = new CGI;
$name = $query->param('name');
$stallAt = $query->param('stallAt');
$stallFor = $query->param('stallFor');
$mimeType = $query->param('mimeType');

my $filesize = stat($name)->size;
print "Cache-Control: no-store\n";
print "Content-type: " . $mimeType . "\n"; 
print "Content-Length: " . $filesize . "\n\n";

open FILE, $name or die;
binmode FILE;
$total = 0;
my ($buf, $data, $n);
while (($n = read FILE, $data, 1024) != 0) {
    $total += $n;
    if ($total > $stallAt) {
        if (defined $stallFor) {
            Time::HiRes::sleep($stallFor)
        }
        last;
    }
    print $data;
}
close(FILE);
