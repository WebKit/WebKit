#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

sub addSlashes
{
    my ($str) = @_;
    $str =~ s/(['"\\])/\\$1/g;
    $str =~ s/([\0])/\\0/g;
    return $str;
}

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print addSlashes($cgi->param('q'));
print "</body>\n";
print "</html>\n";
