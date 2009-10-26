#!/usr/bin/perl -w

print "Content-type: text/html\n\n"; 

print "<p>Test for <a href='https://bugs.webkit.org/show_bug.cgi?id=30723'>bug 30723</a>: Input names added to multipart/form-data headers need to be escaped</p>";
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
print "if (pre.textContent.match('\\nContent-Type'))\n";
print "  document.write('FAIL')\n";
print "else\n";
print "  document.write('PASS')\n";
print "if (window.layoutTestController) {\n";
print "  pre.setAttribute('style', 'display:none');\n";
print "  layoutTestController.notifyDone();\n";
print "}\n";
print "</script>\n";
