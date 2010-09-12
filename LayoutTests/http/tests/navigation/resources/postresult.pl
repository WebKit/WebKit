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
$submitWithPost = $query->param('submitwithpost');
$submitWithPostRedirect = $query->param('submitwithpostredirect');
$submitWithPostRedirectReload = $query->param('submitwithpostredirectreload');
$redirectHappened = $query->param('redirectHappened');
$method = $query->request_method();

if (($submitWithPost && $method eq "POST") || ($redirectHappened && $method eq "GET")) {

    $textFieldData = $query->param('textfield1');

    print "Content-type: text/html\r\n";
    print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    print "\r\n";

    print <<HERE_DOC_END;
    <html>
    <body style="font-size: 32">
    This is just a minimal page that we navigate in response to an HTTP POST.
    <br>
    <br>
    If the next line is empty after the colon, it probably means that we made
    a mistake and requested this page with a GET with no query instead of a POST.
    <br>
    <br>
    The first text field contained: $textFieldData
    <br>
    This page was requested with an HTTP $method
    <script>
    if (window.layoutTestController)
        layoutTestController.notifyDone();
    </script>
    </body>
    </html>
HERE_DOC_END

} elsif ($submitWithPostRedirectReload && $method eq "POST") {

    print "Status: 303 See Other\r\n";
    print "Location: reloadresult.pl\r\n";
    print "Content-type: text/html\r\n";
    print "\r\n";

    print <<HERE_DOC_END
    <html>
    <body style="font-size: 32">
    This page should not be seen - it is a 303 redirect to another page.
    </body>
    </html>
HERE_DOC_END

} elsif ($submitWithPostRedirect && $method eq "POST") {

    $urlQuery = $query->query_string;
    $urlQuery =~ s/;/&/g;       # cheapo conversion from POST data to URL query format
    # redirect to the same page, with an added param to show we've already been here
    $urlQuery .= "&redirectHappened=true";

    print "Status: 303 See Other\r\n";
    printf "Location: postresult.pl?%s\r\n", $urlQuery;
    print "Content-type: text/html\r\n";
    print "\r\n";

    print <<HERE_DOC_END
    <html>
    <body style="font-size: 32">
    This page should not be seen - it is a 303 redirect to another page.
    </body>
    </html>
HERE_DOC_END

} else {

    $queryString = $query->query_string;
    print "Content-type: text/html\r\n";
    print "\r\n";

    print <<HERE_DOC_END
    <html>
    <body style="font-size: 32">
    Test failure: postresult.pl was called with an unexpected set of parameters.
    <br>
    This page was requested with an HTTP $method
    <br>
    The query parameters are: $queryString
    </body>
    </html>
HERE_DOC_END

}

