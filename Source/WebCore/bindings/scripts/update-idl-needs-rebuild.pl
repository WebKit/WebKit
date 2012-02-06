#!/usr/bin/perl -w
#
# Copyright (C) 2012 Google Inc.  All rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
#

use strict;

use File::Basename;
use Getopt::Long;

my $idlFile;
my $supplementalDependencyFile;

GetOptions('supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'idlFile=s' => \$idlFile);

die "Must specify an IDL file." unless $idlFile;
die "Must specify a supplemental dependency file." unless $supplementalDependencyFile;

# Touches .idl-needs-rebuild if .idl-needs-rebuild is older than this IDL file.
my $idlNeedsRebuildFile = basename($idlFile) . "-needs-rebuild";
my $timeStampOfIdlNeedsRebuildFile = timestamp($idlNeedsRebuildFile);
if ($timeStampOfIdlNeedsRebuildFile < timestamp($idlFile)) {
    touch($idlNeedsRebuildFile);
    exit;
}


# The format of a supplemental dependency file:
#
# DOMWindow.idl(1000) P.idl(800) Q.idl(1200) R.idl(1000)
# Document.idl(1000) S.idl(800)
# Event.idl(1200)
# ...
#
# The above indicates that DOMWindow.idl is supplemented by P.idl, Q.idl and R.idl,
# Document.idl is supplemented by S.idl, and Event.idl is supplemented by no IDLs.
# The IDL that supplements another IDL (e.g. P.idl) never appears in the dependency file.
# The number in () is the last access timestamp of the file.
open FH, "<", $supplementalDependencyFile or die "Couldn't open $supplementalDependencyFile: $!";
while (my $line = <FH>) {
    my ($idlFileEntry, @supplementalIdlFileEntries) = split(/\s+/, $line);
    die "The format of supplemental.dep is wrong\n" unless $idlFileEntry =~ /^([^\(]*)\((\d+)\)$/;
    next if (basename($1) ne basename($idlFile));
    my $timeStampOfIdlFile = $2;
    # Touches .idl-needs-rebuild if there is at least one supplemental IDL file which is newer than this IDL file.
    for my $supplementalIdlFileEntry (@supplementalIdlFileEntries) {
        die "The format of supplemental.dep is wrong\n" unless $supplementalIdlFileEntry =~ /^(?:[^\(]*)\((\d+)\)$/;
        my $timeStampOfSupplementalIdlFile = $1;
        if ($timeStampOfIdlNeedsRebuildFile < $timeStampOfSupplementalIdlFile) {
            touch($idlNeedsRebuildFile);
            last;
        }
    }
    last;
}
close FH;

sub touch
{
    my $file = shift;

    open FH, ">", $file or die "Couldn't open $file: $!";
    my $basename = basename($file);
    print FH "This file is used by build scripts only. The timestamp of this file determines whether ${basename}.idl should be rebuilt or not.\n\n";
    print FH "IDLFile=$idlFile\n";
    close FH;
}

sub timestamp
{
    my $file = shift;
    return 0 if ! -e $file;
    return (stat $file)[9];
}
