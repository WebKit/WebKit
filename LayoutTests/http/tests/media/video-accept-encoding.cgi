#!/usr/bin/perl -w

use strict;

use CGI;
use File::stat;

use constant CHUNK_SIZE_BYTES => 1024;

my $query = new CGI;

my $name = $query->param('name') or die;
my $filesize = stat($name)->size;

# Get MIME type.
my $type = $query->param('type') or die;

# Get throttling rate, assuming parameter is in kilobytes per second.
my $kbPerSec = $query->param('throttle') || 0;
my $chunkPerSec = $kbPerSec * 1024 / CHUNK_SIZE_BYTES;

CGI->nph(1);

my $contentRange = $ENV{'HTTP_RANGE'};

my $rangeEnd = $filesize - 1;
my @parsedRange = (0, $rangeEnd);

my $acceptEncoding = $ENV{'HTTP_ACCEPT_ENCODING'};

my $httpStatus;

if ($acceptEncoding) {
    $httpStatus = "406 Not Acceptable";

    print "Status: " . $httpStatus . "\n";
    print "Connection: close\n";
    print "\n";
} else {
    my $httpContentRange;
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
    print "\n";

    open FILE, $name or die;
    binmode FILE;
    my ($data, $n);
    my $total = $parsedRange[0];

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
}
