#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

# Passing semicolons through the url to this script is problematic. The raw
# form truncates the input and the %-encoded form isn't being decoded. Hence
# this set of hard-coded headers. 
if ($cgi->param('disable-protection')) {
    print "X-XSS-Protection: 0\n";
}
if ($cgi->param('enable-full-block')) {
    print "X-XSS-Protection: 1; mode=block\n";
}
if ($cgi->param('enable-report')) {
    print "X-XSS-Protection: 1; report=/security/contentSecurityPolicy/resources/save-report.py?test=" . $cgi->param('test') . "\n";
}
if ($cgi->param('enable-full-block-report')) {
    print "X-XSS-Protection: 1; mode=block; report=/security/contentSecurityPolicy/resources/save-report.py?test=" . $cgi->param('test') . "\n";
}
if ($cgi->param('enable-report-with-redirect')) {
    print "X-XSS-Protection: 1; report=/security/contentSecurityPolicy/resources/save-report-and-redirect-to-save-report.py?test=" . $cgi->param('test') . "\n";
}
if ($cgi->param('enable-full-block-report-with-redirect')) {
    print "X-XSS-Protection: 1; mode=block; report=/security/contentSecurityPolicy/resources/save-report-and-redirect-to-save-report.py?test=" . $cgi->param('test') . "\n";
}


if ($cgi->param('valid-header')) {
    if ($cgi->param('valid-header') == 1) {
        print "X-XSS-Protection:   1  ;MoDe =  bLocK   \n";
    }
    if ($cgi->param('valid-header') == 2) {
        print "X-XSS-Protection: 1; \n";
    }
    if ($cgi->param('valid-header') == 3) {
        print "X-XSS-Protection: 1; mode=block; \n";
    }
    if ($cgi->param('valid-header') == 4) {
        print "X-XSS-Protection: 1; report=/security/contentSecurityPolicy/resources/nonesuch.php; mode=block; \n";
    }
}

if ($cgi->param('malformed-header')) {
    if ($cgi->param('malformed-header') == 1) {
        print "X-XSS-Protection: 12345678901234567\n";
    }
    if ($cgi->param('malformed-header') == 2) {
        print "X-XSS-Protection: red\n";
    }
    if ($cgi->param('malformed-header') == 3) {
        print "X-XSS-Protection: 1; mode=purple\n";
    }
    if ($cgi->param('malformed-header') == 4) {
        print "X-XSS-Protection: 1; mode=block-a-block-block\n";
    }
    if ($cgi->param('malformed-header') == 5) {
        print "X-XSS-Protection: 1; mode=block; report\n";
    }
    if ($cgi->param('malformed-header') == 6) {
        print "X-XSS-Protection: 1; report= ;\n";
    }
    if ($cgi->param('malformed-header') == 7) {
        print "X-XSS-Protection: 1; red\n";
    }
    if ($cgi->param('malformed-header') == 8) {
        print "X-XSS-Protection: 1; mode=block; report=/fail; mode=block;\n";
    }
    if ($cgi->param('malformed-header') == 9) {
        print "X-XSS-Protection: 1; mode=block; report=/fail; report=/fail;\n";
    }
}

print "Content-Type: text/html; charset=";
print $cgi->param('charset') ? $cgi->param('charset') : "UTF-8";
print "\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
if ($cgi->param('wait-for-load')) {
    print "<script>\n";
     print "onload = function() {\n";
    print "    window.parent.postMessage('loaded', '*');\n";
    print "}\n";
    print "</script>\n";
}
if ($cgi->param('inHead')) {
    print "<head>\n";
} else {
    print "<body>\n";
}
if ($cgi->param('replaceState')) {
    print "<script>history.replaceState({}, '', '#must-not-appear');</script>\n";
}
print $cgi->param('q'); # XSS reflected here.
if ($cgi->param('script-expression-follows')) {
    print "\n <script>42;</script>\n";
}
if ($cgi->param('clutter')) {
    print $cgi->param('clutter');
}
if ($cgi->param('q2')) {
    print $cgi->param('q2');
}
if ($cgi->param('showAction')) {
    print "<script>\n";
    print "    console.log('Form action set to ' + document.forms[0].action);\n";
    print "</script>\n";
}
if ($cgi->param('showFormaction')) {
    print "<script>\n";
    print "    var e = document.querySelector('[formaction]');\n";
    print "    if (e)\n";
    print "        console.log('formaction present on ' + e.nodeName + ' with value of ' + e.getAttribute('formaction'));\n";
    print "</script>\n";
}
if ($cgi->param('dumpElementBySelector')) {
    print "<pre id='console'></pre>\n";
    print "<script>\n";
    print "    var e = document.querySelector('" . $cgi->param('dumpElementBySelector') . "');\n";
    print "    if (e) {\n";
    print "        var log = '" . $cgi->param('dumpElementBySelector') . " => ' + e.nodeName + '\\n';\n";
    print "        for (var i = 0; i < e.attributes.length; i++) {\n";
    print "            log += '* ' + e.attributes[i].name + ': ' + e.attributes[i].value + '\\n';\n";
    print "        }\n";
    print "        document.getElementById('console').innerText = log;\n";
    print "    } else\n";
    print "        console.log('No element matched the given selector.');\n";
    print "</script>\n";
}
if ($cgi->param('notifyDone')) {
    print "<script>\n";
    print "if (window.testRunner)\n";
    print "    testRunner.notifyDone();\n";
    print "</script>\n";
}
if ($cgi->param('enable-full-block') || $cgi->param('enable-full-block-report') || $cgi->param('enable-full-block-report-with-redirect')) {
    print "<p>If you see this message then the test FAILED.</p>\n";
}
if ($cgi->param('alert-cookie')) {
    print "<script>if (/xssAuditorTestCookie/.test(document.cookie)) { console.log('FAIL: ' + document.cookie); document.cookie = 'xssAuditorTestCookie=remove; max-age=-1'; } else console.log('PASS');</script>\n";
}
if ($cgi->param('echo-report')) {
    print "<script src=/security/contentSecurityPolicy/resources/go-to-echo-report.py?test=" . $cgi->param('test') . "></script>\n";
}
if ($cgi->param('inHead')) {
    print "</head>\n";
} else {
    print "</body>\n";
}
print "</html>\n";
