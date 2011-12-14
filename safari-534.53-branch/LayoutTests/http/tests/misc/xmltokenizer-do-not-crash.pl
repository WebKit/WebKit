#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/xml\r\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "Pragma: no-cache\r\n";
print "\r\n";

print "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n";
print "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"pl\">\n";
print "<head>\n";
print "<script>\n";
print "if (window.layoutTestController) layoutTestController.dumpAsText();\n";
print "</script>";
print "</head>\n";
print "<body>\n";
print "<pre id='msg'></pre>\n";
print "<script type='text/javascript' src='resources/script-slow1.pl'></script>\n";
sleep 1;
print "<script type='text/javascript' src='resources/script-slow2.pl'></script>\n";
print "</body></html>\n";
