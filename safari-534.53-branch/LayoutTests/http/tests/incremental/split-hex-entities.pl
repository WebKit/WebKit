#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/html\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "<body>\n";
print "<script>\n";
print "    if (window.layoutTestController)\n";
print "        layoutTestController.dumpAsText();\n";
print "</script>\n";
print "<p>Test for <a href=\"http://bugzilla.opendarwin.org/show_bug.cgi?id=4820\">bug 4820</a>: hexadecimal HTML entities split across TCP packets are not parsed correctly.</p>";
print "<p>Should be a blank page (except for this description).</p>";

for ($count=1; $count<3000; $count++) {
    print "&#x0020;&#x0020; &#x0020;&#x0020;&#x0020;&#x0020;&#x20;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;  ";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; &#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x020;&#x0020;&#x0020;&#x0020;&#x020;&#x0020;&#x0020;&#x0020;";
    print "&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x20;&#x0020;&#x0020;&#x0020; &#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
    print "&#x0020;&#x0020;&#x0020; &#x20; &#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020;&#x0020; ";
}
print "</body>\n";
