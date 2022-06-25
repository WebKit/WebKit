#!/usr/bin/perl

print "Content-type: text/html\r\n";

@keypairs = split(/&/, $ENV{'QUERY_STRING'});

$type = "";

foreach $pair (@keypairs)
{
    ($name, $value) = split(/=/, $pair);
    
    if ($name eq "type") {
        $type = $value;
    }
}

print "Set-Cookie: reload-subframe-$type=1\r\n";
print "\r\n";

print "<html>";
print "<head>";
print "</head>";

@cookies = split(/;/, $ENV{'HTTP_COOKIE'});

$subframe_content = "Fail";

foreach $pair (@cookies)
{
    ($name, $value) = split(/=/, $pair);
    
    $name =~ s/^\s+//;
    $name =~ s/\s+$//;
    
    if ($name eq "reload-subframe-$type") {
        $subframe_content = "Pass";
    }
}

$src = "'reload-subframe-content.pl?text=$subframe_content'";

if ($type eq "iframe") {
    print "<body>";
    print "<iframe src=$src></iframe>";
    print "</body>";
} elsif ($type eq "object") {
    print "<body>";
    print "<object data=$src></object>";
    print "</body>";
} else {
    print "<frameset>";
    print "<frame src=$src>";
    print "</frameset>";
}

print "</html>";
