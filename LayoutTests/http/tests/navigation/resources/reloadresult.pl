#!/usr/bin/perl
# Simple script to generate a POST result.
#
# Depending on which button was pushed in the form, we either generate a direct
# result page, or we use the pattern where the post returns a 303 redirect,
# and then the resulting GET yields the true POST result.  Sites do this trick
# to avoid having POSTS in the b/f list, so you don't run into POSTs getting
# resubmitted by accident.

use CGI;
$query = new CGI;
$method = $query->request_method();

if ($method eq "POST") {

    print "Content-type: text/html\r\n";
    print "\r\n";

    print <<HERE_DOC_END
    <html>
    Test failure: reloadresult.pl was called with an unexpected method ($method).
    </body>
    </html>
HERE_DOC_END

} elsif ($method eq "GET") {

    print "Content-type: text/html\r\n";
    print "\r\n";

    print <<HERE_DOC_END
    <html>
    <body>
    PASS
    </body>
    </html>
HERE_DOC_END

}
