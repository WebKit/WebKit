#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# Contributor(s): Joel Peshkin <bugreport@peshkin.net>
#                 Byron Jones <byron@glob.com.au>

# testserver.pl is involked with the baseurl of the Bugzilla installation
# as its only argument.  It attempts to troubleshoot as many installation
# issues as possible.

use Socket;
use Bugzilla::Config qw($datadir);
my $envpath = $ENV{'PATH'};
use lib ".";
use strict;
require "globals.pl";
eval "require LWP; require LWP::UserAgent;";
my $lwp = $@ ? 0 : 1;

$ENV{'PATH'}= $envpath;

if ((@ARGV != 1) || ($ARGV[0] !~ /^https?:/))
{
    print "Usage: $0 <URL to this Bugzilla installation>\n";
    print "e.g.:  $0 http://www.mycompany.com/bugzilla\n";
    exit(1);
}


# Try to determine the GID used by the webserver.
my @pscmds = ('ps -eo comm,gid', 'ps -acxo command,gid', 'ps -acxo command,rgid');
my $sgid = 0;
if ($^O !~ /MSWin32/i) {
    foreach my $pscmd (@pscmds) {
        open PH, "$pscmd 2>/dev/null |";
        while (my $line = <PH>) {
            if ($line =~ /^(?:\S*\/)?(?:httpd|apache)2?\s+(\d+)$/) {
                $sgid = $1 if $1 > $sgid;
            }
        }
        close(PH);
    }
}

# Determine the numeric GID of $webservergroup
my $webgroupnum = 0;
if ($::webservergroup =~ /^(\d+)$/) {
    $webgroupnum = $1;
} else {
    eval { $webgroupnum = (getgrnam $::webservergroup) || 0; };
}

# Check $webservergroup against the server's GID
if ($sgid > 0) {
    if ($::webservergroup eq "") {
        print 
"WARNING \$webservergroup is set to an empty string.
That is a very insecure practice. Please refer to the
Bugzilla documentation.\n";
    } elsif ($webgroupnum == $sgid) {
        print "TEST-OK Webserver is running under group id in \$webservergroup.\n";
    } else {
        print 
"TEST-WARNING Webserver is running under group id not matching \$webservergroup.
This if the tests below fail, this is probably the problem.
Please refer to the webserver configuration section of the Bugzilla guide. 
If you are using virtual hosts or suexec, this warning may not apply.\n";
    }
} elsif ($^O !~ /MSWin32/i) {
   print
"TEST-WARNING Failed to find the GID for the 'httpd' process, unable
to validate webservergroup.\n";
}


# Try to fetch a static file (ant.jpg)
$ARGV[0] =~ s/\/$//;
my $url = $ARGV[0] . "/ant.jpg";
if (fetch($url)) {
    print "TEST-OK Got ant picture.\n";
} else {
    print 
"TEST-FAILED Fetch of ant.jpg failed
Your webserver could not fetch $url.
Check your webserver configuration and try again.\n";
    exit(1);
}

# Try to execute a cgi script
my $response = fetch($ARGV[0] . "/testagent.cgi");
if ($response =~ /^OK/) {
    print "TEST-OK Webserver is executing CGIs.\n";
} elsif ($response =~ /^#!/) {
    print 
"TEST-FAILED Webserver is fetching rather than executing CGI files.
Check the AddHandler statement in your httpd.conf file.\n";
    exit(1);
} else {
    print "TEST-FAILED Webserver is not executing CGI files.\n"; 
}

# Make sure that webserver is honoring .htaccess files
$::localconfig =~ s~^\./~~;
$url = $ARGV[0] . "/$::localconfig";
$response = fetch($url);
if ($response) {
    print 
"TEST-FAILED Webserver is permitting fetch of $url.
This is a serious security problem.
Check your webserver configuration.\n";
    exit(1);
} else {
    print "TEST-OK Webserver is preventing fetch of $url.\n";
}

eval 'use GD';
if ($@ eq '') {
    undef $/;

    # Ensure major versions of GD and libgd match
    # Windows's GD module include libgd.dll, guarenteed to match

    if ($^O !~ /MSWin32/i) {
        my $gdlib = `gdlib-config --version 2>&1`;
        $gdlib =~ s/\n$//;
        if (!$gdlib) {
            print "TEST-WARNING Failed to run gdlib-config, assuming gdlib " .
                  "version 1.x\n";
            $gdlib = '1.x';
        }
        my $gd = $GD::VERSION;

        my $verstring = "GD version $gd, libgd version $gdlib";

        $gdlib =~ s/^([^\.]+)\..*/$1/;
        $gd =~ s/^([^\.]+)\..*/$1/;

        if ($gdlib == $gd) {
            print "TEST-OK $verstring; Major versions match.\n";
        } else {
            print "TEST-FAIL $verstring; Major versions do not match\n";
        }
    }

    # Test GD

    eval {
        my $image = new GD::Image(100, 100);
        my $black = $image->colorAllocate(0, 0, 0);
        my $white = $image->colorAllocate(255, 255, 255);
        my $red = $image->colorAllocate(255, 0, 0);
        my $blue = $image->colorAllocate(0, 0, 255);
        $image->transparent($white);
        $image->rectangle(0, 0, 99, 99, $black);
        $image->arc(50, 50, 95, 75, 0, 360, $blue);
        $image->fill(50, 50, $red);

        if ($image->can('png')) {
            create_file("$datadir/testgd-local.png", $image->png);
            check_image("$datadir/testgd-local.png", 't/testgd.png', 'GD', 'PNG');
        } else {
            die "GD doesn't support PNG generation\n";
        }
    };
    if ($@ ne '') {
        print "TEST-FAILED GD returned: $@\n";
    }

    # Test Chart

    eval 'use Chart::Lines';
    if ($@) {
        print "TEST-FAILED Chart::Lines is not installed\n";
    } else {
        eval {
            my $chart = Chart::Lines->new(400, 400);

            $chart->add_pt('foo', 30, 25);
            $chart->add_pt('bar', 16, 32);

            my $type = $chart->can('gif') ? 'gif' : 'png';
            $chart->$type("$datadir/testchart-local.$type");
            check_image("$datadir/testchart-local.$type", "t/testchart.$type",
                "Chart", uc($type));
        };
        if ($@ ne '') {
            print "TEST-FAILED Chart returned: $@\n";
        }
    }
}

sub fetch {
    my $url = shift;
    my $rtn;
    if ($lwp) {
        my $req = HTTP::Request->new(GET => $url);
        my $ua = LWP::UserAgent->new;
        my $res = $ua->request($req);
        $rtn = ($res->is_success ? $res->content : undef);
    } elsif ($url =~ /^https:/i) {
        die("You need LWP installed to use https with testserver.pl");
    } else {
        my($host, $port, $file) = ('', 80, '');
        if ($url =~ m#^http://([^:]+):(\d+)(/.*)#i) {
            ($host, $port, $file) = ($1, $2, $3);
        } elsif ($url =~ m#^http://([^/]+)(/.*)#i) {
            ($host, $file) = ($1, $2);
        } else {
            die("Cannot parse url");
        }

        my $proto = getprotobyname('tcp');
        socket(SOCK, PF_INET, SOCK_STREAM, $proto);
        my $sin = sockaddr_in($port, inet_aton($host));
        if (connect(SOCK, $sin)) {
            binmode SOCK;
            select((select(SOCK), $| = 1)[0]);

            # get content
    
            print SOCK "GET $file HTTP/1.0\015\012host: $host:$port\015\012\015\012";
            my $header = '';
            while (defined(my $line = <SOCK>)) {
                last if $line eq "\015\012";
                $header .= $line;
            }
            my $content = '';
            while (defined(my $line = <SOCK>)) {
                $content .= $line;
            }

            my ($status) = $header =~ m#^HTTP/\d+\.\d+ (\d+)#;
            $rtn = (($status =~ /^2\d\d/) ? $content : undef);
        }
    }
    return($rtn);
}

sub check_image {
    my ($local_file, $test_file, $library, $image_type) = @_;
    if (read_file($local_file) eq read_file($test_file)) {
        print "TEST-OK $library library generated a good $image_type image\n";
        unlink $local_file;
    } else {
        print "TEST-WARNING $library library generated a $image_type that " .
              "didn't match the expected image.\nIt has been saved as " .
              "$local_file and should be compared with $test_file\n";
    }
}

sub create_file {
    my ($filename, $content) = @_;
    open(FH, ">$filename")
        or die "Failed to create $filename: $!\n";
    binmode FH;
    print FH $content;
    close FH;
}

sub read_file {
    my ($filename) = @_;
    open(FH, $filename)
        or die "Failed to open $filename: $!\n";
    binmode FH;
    my $content = <FH>;
    close FH;
    return $content;
}
