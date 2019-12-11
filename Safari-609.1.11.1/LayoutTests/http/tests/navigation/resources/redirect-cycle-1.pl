#!/usr/bin/perl

$count = 1;

@cookies = split(/;/, $ENV{'HTTP_COOKIE'});
foreach $pair (@cookies)
{
    ($name, $value) = split(/=/, $pair);
    
    $name =~ s/^\s+//;
    $name =~ s/\s+$//;
    
    if ($name eq "redirect-cycle-count") {
        $count = $value;
    }
}

if ($count eq 1) {
    print "Status: 302 Moved Temporarily\r\n";
    print "Location: redirect-cycle-2.pl\r\n";
    print "Content-type: text/html\r\n";
    print "Set-Cookie: redirect-cycle-count=2\r\n";
    print "\r\n";
    print "<html>";
    print "<body>";
    print "<div>Page 1</div>";
    print "</body>";
    print "</html>";
} else {
    print "Content-type: text/html\r\n";
    print "\r\n";
    print "<html>";
    print "<head>";
    print "<script type='text/javascript'>";
    print "function startTest() { testRunner.dumpBackForwardList(); }";
    print "</script>";
    print "</head>";
    print "<body onload='startTest();'>";
    print "<div>Page 3</div>";
    print "</body>";
    print "</html>";
}
