#!/usr/bin/perl -w

use strict;

use CGI;
use File::stat;

use constant CHUNK_SIZE_BYTES => 1024;

my $query = new CGI;

my $name = $query->param('name');
my $filesize = stat($name)->size;

# Get throttling rate, assuming parameter is in kilobytes per second.
my $kbPerSec = $query->param('throttle');
my $chunkPerSec = $kbPerSec * 1024 / CHUNK_SIZE_BYTES;

# Get MIME type if provided.  Default to video/mp4.
my $type = $query->param('type') || "video/mp4";

my $nph = $query->param('nph') || 0;
CGI->nph($nph);

my $contentRange = $ENV{'HTTP_RANGE'};

my $rangeEnd = $filesize - 1;
my @parsedRange = (0, $rangeEnd);

if ($nph) {
    # Handle HTTP Range requests.
    my $httpContentRange;
    my $httpStatus;

    if ($contentRange) {
        my @values = split('=', $contentRange);
        my $rangeType = $values[0];
        @parsedRange = split("-", $values[1]);

        if (!$parsedRange[1]) {
            $parsedRange[1] = $rangeEnd;
        }
        $httpStatus = "206 Partial Content";
        $httpContentRange = "bytes " . $parsedRange[0] . "-" . $parsedRange[1] . "/" . $filesize;
    } else {
        $httpStatus = "200 OK";
    }

    print "Status: " . $httpStatus . "\n";
    print "Connection: close\n";
    print "Content-Length: " . $filesize . "\n";
    print "Content-Type: " . $type . "\n";
    print "Accept-Ranges: bytes\n";
    if ($httpContentRange) {
        print "Content-Range: " . $httpContentRange . "\n";
    }
} else {
    # Print HTTP Header, disabling cache.
    print "Cache-Control: no-cache\n";
    print "Content-Length: " . $filesize . "\n";
    print "Content-Type: " . $type . "\n";
}

print "\n";

open FILE, $name or die;
binmode FILE;
my ($data, $n);
my $total = 0;

seek(FILE, $parsedRange[0], 0);

while (($n = read FILE, $data, 1024) != 0) {
    print $data;

    $total += $n;
    if (($total >= $filesize) || ($total > $parsedRange[1])) {
        last;
    }

    # Throttle if there is some.
    if ($chunkPerSec > 0) {
        select(undef, undef, undef, 1.0 / $chunkPerSec);
    }
}
close(FILE);
