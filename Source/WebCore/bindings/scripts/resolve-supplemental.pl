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

use IDLParser;

my $defines;
my $preprocessor;
my $verbose;
my $idlFilesList;
my $supplementalDependencyFile;

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'verbose' => \$verbose,
           'idlFilesList=s' => \$idlFilesList,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile);

die('Must specify #define macros using --defines.') unless defined($defines);
die('Must specify an output file using --supplementalDependencyFile.') unless defined($supplementalDependencyFile);
die('Must specify the file listing all IDLs using --idlFilesList.') unless defined($idlFilesList);

if ($verbose) {
    print "Resolving [Supplemental=XXX] dependencies in all IDL files.\n";
}

open FH, "< $idlFilesList" or die "Cannot open $idlFilesList\n";
my @idlFiles = <FH>;
chomp(@idlFiles);
close FH;

# Parse all IDL files.
my %documents;
my %interfaceNameToIdlFile;
foreach my $idlFile (@idlFiles) {
    my $fullPath = Cwd::realpath($idlFile);
    my $parser = IDLParser->new(!$verbose);
    $documents{$fullPath} = $parser->Parse($idlFile, $defines, $preprocessor);
    $interfaceNameToIdlFile{fileparse(basename($idlFile), ".idl")} = $fullPath;
}

# Resolves [Supplemental=XXX] dependencies.
my %supplementals;
foreach my $idlFile (keys %documents) {
    $supplementals{$idlFile} = [];
}
foreach my $idlFile (keys %documents) {
    foreach my $dataNode (@{$documents{$idlFile}->classes}) {
        if ($dataNode->extendedAttributes->{"Supplemental"}) {
            my $targetIdlFile = $interfaceNameToIdlFile{$dataNode->extendedAttributes->{"Supplemental"}};
            push(@{$supplementals{$targetIdlFile}}, $idlFile);
            # Treats as if this IDL file does not exist.
            delete $supplementals{$idlFile};
        }
    }
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
