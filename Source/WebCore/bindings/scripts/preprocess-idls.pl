#!/usr/bin/perl -w
#
# Copyright (C) 2011 Google Inc.  All rights reserved.
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
use Cwd;

my $defines;
my $preprocessor;
my $idlFilesList;
my $supplementalDependencyFile;
my $supplementalMakefileDeps;

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'idlFilesList=s' => \$idlFilesList,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'supplementalMakefileDeps=s' => \$supplementalMakefileDeps);

die('Must specify #define macros using --defines.') unless defined($defines);
die('Must specify an output file using --supplementalDependencyFile.') unless defined($supplementalDependencyFile);
die('Must specify the file listing all IDLs using --idlFilesList.') unless defined($idlFilesList);

open FH, "< $idlFilesList" or die "Cannot open $idlFilesList\n";
my @idlFiles = <FH>;
chomp(@idlFiles);
close FH;

# Parse all IDL files.
my %interfaceNameToIdlFile;
my %idlFileToInterfaceName;
my %supplementalDependencies;
my %supplementals;
foreach my $idlFile (@idlFiles) {
    my $fullPath = Cwd::realpath($idlFile);
    my $supplemental = getSupplementalFromIDLFile($fullPath);
    if ($supplemental) {
        $supplementalDependencies{$fullPath} = $supplemental;
    }
    my $interfaceName = fileparse(basename($idlFile), ".idl");
    $interfaceNameToIdlFile{$interfaceName} = $fullPath;
    $idlFileToInterfaceName{$fullPath} = $interfaceName;
    $supplementals{$fullPath} = [];
}

# Resolves [Supplemental=XXX] dependencies.
foreach my $idlFile (keys %supplementalDependencies) {
    my $baseFile = $supplementalDependencies{$idlFile};
    my $targetIdlFile = $interfaceNameToIdlFile{$baseFile};
    push(@{$supplementals{$targetIdlFile}}, $idlFile);
    delete $supplementals{$idlFile};
}

# Outputs the dependency.
# The format of a supplemental dependency file:
#
# DOMWindow.idl P.idl Q.idl R.idl
# Document.idl S.idl
# Event.idl
# ...
#
# The above indicates that DOMWindow.idl is supplemented by P.idl, Q.idl and R.idl,
# Document.idl is supplemented by S.idl, and Event.idl is supplemented by no IDLs.
# The IDL that supplements another IDL (e.g. P.idl) never appears in the dependency file.

open FH, "> $supplementalDependencyFile" or die "Cannot open $supplementalDependencyFile\n";

foreach my $idlFile (sort keys %supplementals) {
    print FH $idlFile, " @{$supplementals{$idlFile}}\n";
}
close FH;


if ($supplementalMakefileDeps) {
    open MAKE_FH, "> $supplementalMakefileDeps" or die "Cannot open $supplementalMakefileDeps\n";
    my @all_dependencies = [];
    foreach my $idlFile (sort keys %supplementals) {
        my $basename = $idlFileToInterfaceName{$idlFile};

        my @dependencies = map { basename($_) } @{$supplementals{$idlFile}};

        print MAKE_FH "JS${basename}.h: @{dependencies}\n";
        print MAKE_FH "DOM${basename}.h: @{dependencies}\n";
        print MAKE_FH "WebDOM${basename}.h: @{dependencies}\n";
        foreach my $dependency (@dependencies) {
            print MAKE_FH "${dependency}:\n";
        }
    }

    close MAKE_FH;
}


sub getSupplementalFromIDLFile
{
    my $idlFile = shift;

    open FILE, "<", $idlFile;
    my @lines = <FILE>;
    close FILE;

    my $fileContents = join('', @lines);
    while ($fileContents =~ /\[(.*?)\] interface (\w+)/gs) {
        my @attributes = split(',', $1);
        foreach (@attributes) {
            if (/Supplemental=(\w+)/) {
                return $1;
            }
        }
    }
}
