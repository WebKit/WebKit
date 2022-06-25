#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

# We emit two newline characters to signal the end of a text/plain response without HTTP headers.
print "FAIL\n\n";
