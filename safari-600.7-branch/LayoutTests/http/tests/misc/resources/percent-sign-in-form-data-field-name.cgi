#!/usr/bin/perl -w

print "Content-type: text/html\n\n"; 

print "<p>Test for <a href='https://bugs.webkit.org/show_bug.cgi?id=32140'>bug 32140</a>: Mailman administrative functionality is broken.</p>";
print "<pre>";

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'})
                || die "Could not get query\n";
    print $request;
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
} 

print "</pre><script>\n";
print "var pre = document.getElementsByTagName('pre')[0];\n";
print "if (pre.textContent.match('abc%40def'))\n";
print "  document.write('PASS')\n";
print "else\n";
print "  document.write('FAIL')\n";
print "if (window.testRunner) {\n";
print "  pre.setAttribute('style', 'display:none');\n";
print "  testRunner.notifyDone();\n";
print "}\n";
print "</script>\n";
