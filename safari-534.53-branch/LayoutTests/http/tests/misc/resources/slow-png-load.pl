#!/usr/bin/perl -w

use strict;

use CGI;
use File::stat;

use constant CHUNK_SIZE_BYTES => 256;

my $query = new CGI;
my $filename = $query->param('name');
my $filesize = stat($filename)->size;
my $loadtime = $query->param('loadtime'); # in seconds
my $chunkcount = $filesize / CHUNK_SIZE_BYTES;
my $chunkdelay = $loadtime / $chunkcount;

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: image/png\r\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "Pragma: no-cache\r\n";
print "\r\n";

open(FILE, $filename) or die;
binmode FILE;
my ($data, $n);
my $total = 0;

while (($n = read FILE, $data, CHUNK_SIZE_BYTES) != 0) {
    print $data;

    $total += $n;
    if ($total >= $filesize) {
        last;
    }

    # Throttle if there is some.
    if ($chunkdelay > 0) {
        select(undef, undef, undef, $chunkdelay);
    }
}
close(FILE);
