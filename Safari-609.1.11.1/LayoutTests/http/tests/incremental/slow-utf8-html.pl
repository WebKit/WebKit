#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/html\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "\xef\xbb\xbf<body><p>Test for bug 10697: Errors in incremental decoding of UTF-8.</p>\n";
print "<p>Should be a blank page (except for this description).</p>\n";
print "<script>\n";
print "if (window.testRunner)\n";
print "	testRunner.dumpAsText();\n";
print "</script>\n";

# U+2003 = UTF-8 E28083 = EM SPACE
print "\xe2";
for ($count=1; $count<4000; $count++) {
    print "\x80\x83\xe2";
}
print "\x80";
for ($count=1; $count<4000; $count++) {
    print "\x83\xe2\x80";
}
print "\x83";
