#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/html\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "\xef\xbb\xbf<body><p>Bug 21381: Incremental parsing of html causes bogus line numbers in some cases</p>\n";
print "<p>This tests that the line numbers associated with a script element are correct, even when parsing is ";
print "interrupted mid way through the script element</p>\n";
print "<pre id=log></pre>\n";
print "<script>\n";
print "if (window.testRunner)\n";
print "    testRunner.dumpAsText();\n";
for ($count=1; $count<4000; $count++) {
    print "\n";
}
print "try { unknownFunction(); } catch(e) { \n";
print "if (e.line != 4006)\n";
print "    document.getElementById('log').innerText = 'FAIL: Got ' + e.line + ' expected 4006';\n";
print "else \n";
print "    document.getElementById('log').innerText = 'PASS: Got ' + e.line;\n"; 
print "}\n";
print "</script>\n";
