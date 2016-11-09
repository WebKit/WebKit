#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

# We emit two newline characters to signal the end of the response without HTTP headers.
print "postMessage('FAIL');\n\n";
