#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

use strict;
use warnings;

use MIME::Base64;

my $blueSquare = <<'EOF';
iVBORw0KGgoAAAANSUhEUgAAAGQAAABkAQMAAABKLAcXAAAAFmlUWHRYTUw6Y29tLmFkb2JlLnht
cAAAAAAAaEZ+GgAAAAZQTFRFAAD/AAAAe2K/PgAAABl0RVh0U29mdHdhcmUAR3JhcGhpY0NvbnZl
cnRlcjVdSO4AAAA1SURBVHicYmCgK2Ckr3VogJ8GKgcTIDd0h6ZvRzIY2Hw0sACf35nI1Ecd20kA
AAAAAP//AwDMaQA2EUO6XAAAAABJRU5ErkJggg==
EOF
$blueSquare =~ s/\n//g;

print decode_base64($blueSquare);
