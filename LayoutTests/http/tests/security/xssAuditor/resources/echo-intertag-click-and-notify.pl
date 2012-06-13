#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

if ($cgi->param('enable-full-block')) {
    print "X-XSS-Protection: 1; mode=block\n";
}
print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<head>\n";
print "<script>\n";
print "window.onload = function()\n";
print "{\n";
print "    var event = document.createEvent('MouseEvent');\n";
print "    event.initEvent('click', true, true);\n";
print "    document.getElementById('".$cgi->param('elmid')."').dispatchEvent(event);\n";
print "}\n";
print "</script>\n";
print "</head>\n";
print "<body>\n";
print $cgi->param('q');
if ($cgi->param('enable-full-block')) {
    # Note, when testing a full-page-block, we can't call testRunner.notifyDone()
    # on the generated page because it takes some time for the frame to be redirected to
    # about:blank. Hence, the caller of this Perl script must call testRunner.notifyDone()
    # after the redirect has occurred.
    print "<p>If you see this message then the test FAILED.</p>\n";
} else {
    print "<script>\n";
    print "if (window.testRunner)\n";
    print "    testRunner.notifyDone();\n";
    print "</script>\n";
}
print "</body>\n";
print "</html>\n";
