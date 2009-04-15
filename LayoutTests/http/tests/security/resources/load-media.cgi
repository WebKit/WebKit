#!/usr/bin/perl -w

use CGI;
use File::stat;

$query = new CGI;

my $serverPath = $ENV {'DOCUMENT_ROOT'};
$serverPath =~ s/\/$//;

my $type = $query->param('type');
if (!$type) {
    $type = "video/mp4";
}

## get the url to load (relative to server root, not cgi)
my $url = $query->param('url');
$url = $serverPath . $url;

my $filesize = stat($url)->size;
open FILE, $url or die;
binmode FILE;

print "Content-type: " . $type . "\n";
print "Content-Length: " . $filesize . "\n\n";

my ($buf, $data, $n);
while (($n = read FILE, $data, 1024) != 0) {
    print $data;
}
close(FILE);
