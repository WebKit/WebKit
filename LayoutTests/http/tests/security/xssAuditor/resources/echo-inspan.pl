#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<html>\n";
print "<head>\n";
print '<meta http-equiv="Content-Type" content="text/html;charset=UTF-8">\n';
print "</head>\n";
print "<body>\n";
print "<span>";
print $cgi->param('q');
print "</span>hi there<script>hello=1;</script>\n";
print "</body></html>\n";
