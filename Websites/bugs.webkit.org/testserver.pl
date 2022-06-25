#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# testserver.pl is invoked with the baseurl of the Bugzilla installation
# as its only argument.  It attempts to troubleshoot as many installation
# issues as possible.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;

use Socket;

my $datadir = bz_locations()->{'datadir'};

eval "require LWP; require LWP::UserAgent;";
my $lwp = $@ ? 0 : 1;

if ((@ARGV != 1) || ($ARGV[0] !~ /^https?:/i))
{
    say "Usage: $0 <URL to this Bugzilla installation>";
    say "e.g.:  $0 http://www.mycompany.com/bugzilla";
    exit(1);
}


# Try to determine the GID used by the web server.
my @pscmds = ('ps -eo comm,gid', 'ps -acxo command,gid', 'ps -acxo command,rgid');
my $sgid = 0;
if (!ON_WINDOWS) {
    foreach my $pscmd (@pscmds) {
        open PH, '-|', "$pscmd 2>/dev/null";
        while (my $line = <PH>) {
            if ($line =~ /^(?:\S*\/)?(?:httpd|apache?)2?\s+(\d+)$/) {
                $sgid = $1 if $1 > $sgid;
            }
        }
        close(PH);
    }
}

# Determine the numeric GID of $webservergroup
my $webgroupnum = 0;
my $webservergroup = Bugzilla->localconfig->{webservergroup};
if ($webservergroup =~ /^(\d+)$/) {
    $webgroupnum = $1;
}
else {
    eval { $webgroupnum = (getgrnam $webservergroup) || 0; };
}

# Check $webservergroup against the server's GID
if ($sgid > 0) {
    if ($webservergroup eq "") {
        say 
"WARNING \$webservergroup is set to an empty string.
That is a very insecure practice. Please refer to the
Bugzilla documentation.";
    }
    elsif ($webgroupnum == $sgid || Bugzilla->localconfig->{use_suexec}) {
        say "TEST-OK Webserver is running under group id in \$webservergroup.";
    }
    else {
        say 
"TEST-WARNING Webserver is running under group id not matching \$webservergroup.
This if the tests below fail, this is probably the problem.
Please refer to the web server configuration section of the Bugzilla guide. 
If you are using virtual hosts or suexec, this warning may not apply.";
    }
}
elsif (!ON_WINDOWS) {
   say
"TEST-WARNING Failed to find the GID for the 'httpd' process, unable
to validate webservergroup.";
}


# Try to fetch a static file (padlock.png)
$ARGV[0] =~ s/\/$//;
my $url = $ARGV[0] . "/images/padlock.png";
if (fetch($url)) {
    say "TEST-OK Got padlock picture.";
} else {
    say 
"TEST-FAILED Fetch of images/padlock.png failed
Your web server could not fetch $url.
Check your web server configuration and try again.";
    exit(1);
}

# Try to execute a cgi script
my $response = clean_text(fetch($ARGV[0] . "/testagent.cgi"));
if ($response =~ /^OK (.*)$/) {
    say "TEST-OK Webserver is executing CGIs via $1.";
} elsif ($response =~ /^#!/) {
    say
"TEST-FAILED Webserver is fetching rather than executing CGI files.
Check the AddHandler statement in your httpd.conf file.";
    exit(1);
} else {
    say "TEST-FAILED Webserver is not executing CGI files.";
}

# Make sure that the web server is honoring .htaccess files
my $localconfig = bz_locations()->{'localconfig'};
$localconfig =~ s~^\./~~;
$url = $ARGV[0] . "/$localconfig";
$response = fetch($url);
if ($response) {
    say
"TEST-FAILED Webserver is permitting fetch of $url.
This is a serious security problem.
Check your web server configuration.";
    exit(1);
} else {
    say "TEST-OK Webserver is preventing fetch of $url.";
}

# Test chart generation
eval 'use GD';
if ($@ eq '') {
    undef $/;

    # Ensure major versions of GD and libgd match
    # Windows's GD module include libgd.dll, guaranteed to match
    if (!ON_WINDOWS) {
        my $gdlib = `gdlib-config --version 2>&1` || "";
        $gdlib =~ s/\n$//;
        if (!$gdlib) {
            say "TEST-WARNING Failed to run gdlib-config; can't compare " .
                  "GD versions.";
        }
        else {
            my $gd = $GD::VERSION;
    
            my $verstring = "GD version $gd, libgd version $gdlib";
    
            $gdlib =~ s/^([^\.]+)\..*/$1/;
            $gd =~ s/^([^\.]+)\..*/$1/;
    
            if ($gdlib == $gd) {
                say "TEST-OK $verstring; Major versions match.";
            } else {
                say "TEST-FAILED $verstring; Major versions do not match.";
            }
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
            check_image("$datadir/testgd-local.png", 'GD');
        } else {
            say "TEST-FAILED GD doesn't support PNG generation.";
        }
    };
    if ($@ ne '') {
        say "TEST-FAILED GD returned: $@";
    }

    # Test Chart
    eval 'use Chart::Lines';
    if ($@) {
        say "TEST-FAILED Chart::Lines is not installed.";
    } else {
        eval {
            my $chart = Chart::Lines->new(400, 400);

            $chart->add_pt('foo', 30, 25);
            $chart->add_pt('bar', 16, 32);

            $chart->png("$datadir/testchart-local.png");
            check_image("$datadir/testchart-local.png", "Chart");
        };
        if ($@ ne '') {
            say "TEST-FAILED Chart returned: $@";
        }
    }

    eval 'use Template::Plugin::GD::Image';
    if ($@) {
        say "TEST-FAILED Template::Plugin::GD is not installed.";
    }
    else {
        say "TEST-OK Template::Plugin::GD is installed.";
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
    my ($local_file, $library) = @_;
    my $filedata = read_file($local_file);
    if ($filedata =~ /^\x89\x50\x4E\x47\x0D\x0A\x1A\x0A/) {
        say "TEST-OK $library library generated a good PNG image.";
        unlink $local_file;
    } else {
        say "TEST-WARNING $library library did not generate a good PNG.";
    }
}

sub create_file {
    my ($filename, $content) = @_;
    open(FH, ">", $filename)
        or die "Failed to create $filename: $!\n";
    binmode FH;
    print FH $content;
    close FH;
}

sub read_file {
    my ($filename) = @_;
    open(FH, '<', $filename)
        or die "Failed to open $filename: $!\n";
    binmode FH;
    my $content = <FH>;
    close FH;
    return $content;
}
