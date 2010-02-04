# Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved
# Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
# Copyright (C) 2010 Andras Becsi (abecsi@inf.u-szeged.hu), University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Module to share code to start and stop the Apache daemon.

use strict;
use warnings;

use File::Path;
use File::Spec;
use File::Spec::Functions;
use IPC::Open2;

use webkitdirs;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(&getHTTPDPath &getDefaultConfigForTestDirectory &openHTTPD &closeHTTPD &getHTTPDPid &setShouldWaitForUserInterrupt);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

my $tmpDir = "/tmp";
my $httpdPath;
my $httpdPidDir = File::Spec->catfile($tmpDir, "WebKit");
my $httpdPidFile = File::Spec->catfile($httpdPidDir, "httpd.pid");
my $httpdPid;
my $waitForUserInterrupt = 0;

$SIG{'INT'} = 'cleanup';
$SIG{'TERM'} = 'cleanup';

sub getHTTPDPath
{
    if (isDebianBased()) {
        $httpdPath = "/usr/sbin/apache2";
    } else {
        $httpdPath = "/usr/sbin/httpd";
    }
    return $httpdPath;
}

sub getDefaultConfigForTestDirectory
{
    my ($testDirectory) = @_;
    die "No test directory has been specified." unless ($testDirectory);

    my $httpdConfig;
    getHTTPDPath();
    if (isCygwin()) {
        my $windowsConfDirectory = "$testDirectory/http/conf/";
        unless (-x "/usr/lib/apache/libphp4.dll") {
            copy("$windowsConfDirectory/libphp4.dll", "/usr/lib/apache/libphp4.dll");
            chmod(0755, "/usr/lib/apache/libphp4.dll");
        }
        $httpdConfig = "$windowsConfDirectory/cygwin-httpd.conf";
    } elsif (isDebianBased()) {
        $httpdConfig = "$testDirectory/http/conf/apache2-debian-httpd.conf";
    } elsif (isFedoraBased()) {
        $httpdConfig = "$testDirectory/http/conf/fedora-httpd.conf";
    } else {
        $httpdConfig = "$testDirectory/http/conf/httpd.conf";
        $httpdConfig = "$testDirectory/http/conf/apache2-httpd.conf" if `$httpdPath -v` =~ m|Apache/2|;
    }

    my $documentRoot = "$testDirectory/http/tests";
    my $jsTestResourcesDirectory = $testDirectory . "/fast/js/resources";
    my $typesConfig = "$testDirectory/http/conf/mime.types";
    my $httpdLockFile = File::Spec->catfile($httpdPidDir, "httpd.lock");
    my $httpdScoreBoardFile = File::Spec->catfile($httpdPidDir, "httpd.scoreboard");

    my @httpdArgs = (
        "-f", "$httpdConfig",
        "-C", "DocumentRoot \"$documentRoot\"",
        # Setup a link to where the js test templates are stored, use -c so that mod_alias will already be loaded.
        "-c", "Alias /js-test-resources \"$jsTestResourcesDirectory\"",
        "-c", "TypesConfig \"$typesConfig\"",
        # Apache wouldn't run CGIs with permissions==700 otherwise
        "-c", "User \"#$<\"",
        "-c", "LockFile \"$httpdLockFile\"",
        "-c", "PidFile \"$httpdPidFile\"",
        "-c", "ScoreBoardFile \"$httpdScoreBoardFile\"",
    );

    # FIXME: Enable this on Windows once <rdar://problem/5345985> is fixed
    # The version of Apache we use with Cygwin does not support SSL
    my $sslCertificate = "$testDirectory/http/conf/webkit-httpd.pem";
    push(@httpdArgs, "-c", "SSLCertificateFile \"$sslCertificate\"") unless isCygwin();

    return @httpdArgs;

}

sub openHTTPD(@)
{
    my (@args) = @_;
    die "No HTTPD configuration has been specified" unless (@args);
    mkdir($httpdPidDir, 0755);
    die "No write permissions to $httpdPidDir" unless (-w $httpdPidDir);

    if (-f $httpdPidFile) {
        open (PIDFILE, $httpdPidFile);
        my $oldPid = <PIDFILE>;
        chomp $oldPid;
        close PIDFILE;
        if (0 != kill 0, $oldPid) {
            print "\nhttpd is already running: pid $oldPid, killing...\n";
            kill 15, $oldPid;

            my $retryCount = 20;
            while ((kill(0, $oldPid) != 0) && $retryCount) {
                sleep 1;
                --$retryCount;
            }

            die "Timed out waiting for httpd to quit" unless $retryCount;
        }
    }

    $httpdPath = "/usr/sbin/httpd" unless ($httpdPath);

    open2(">&1", \*HTTPDIN, $httpdPath, @args);

    my $retryCount = 20;
    while (!-f $httpdPidFile && $retryCount) {
        sleep 1;
        --$retryCount;
    }

    if (!$retryCount) {
        rmtree $httpdPidDir;
        die "Timed out waiting for httpd to start";
    }

    $httpdPid = <PIDFILE> if open(PIDFILE, $httpdPidFile);
    chomp $httpdPid if $httpdPid;
    close PIDFILE;

    waitpid($httpdPid, 0) if ($waitForUserInterrupt && $httpdPid);

    return 1;
}

sub closeHTTPD
{
    close HTTPDIN;
    if ($httpdPid) {
        kill 15, $httpdPid;
        my $retryCount = 20;
        while (-f $httpdPidFile && $retryCount) {
            sleep 1;
            --$retryCount;
        }

        if (!$retryCount) {
            print STDERR "Timed out waiting for httpd to terminate!\n";
            return 0;
        }
    }
    rmdir $httpdPidDir;
    return 1;
}

sub setShouldWaitForUserInterrupt
{
    $waitForUserInterrupt = 1;
}

sub cleanup
{
    closeHTTPD();
    print "\n";
    exit(1);
}
