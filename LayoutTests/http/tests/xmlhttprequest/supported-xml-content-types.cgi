#!/usr/bin/perl -w

use CGI qw(:standard);
my $cgi = new CGI;
my $type = $cgi->param('type');
$type =~ s/\^\^PLUS\^\^/+/g;

my $escapedType = $type;
$escapedType =~ s/&/&#38;/;
$escapedType =~ s/</&#60;/;
$escapedType =~ s/>/&#62;/;
$escapedType =~ s/'/&#39;/;
$escapedType =~ s/"/&#34;/;

print "Content-type: $type\n\n";
print "<type>$escapedType</type>\n";
