#!/usr/bin/perl -w
#
# Copyright (C) 2005 Apple Computer, Inc.
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# 
# This file is part of WebKit
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
# aint with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
# 

# This script is a temporary hack.
# Files are generated in the source directory, when they really should go
# to the DerivedSources directory.
# This should also eventually be a build rule driven off of .idl files
# however a build rule only solution is blocked by several radars:
# <rdar://problems/4251781&4251785>

use strict;

use File::Path;
use Getopt::Long;
use Cwd;

use IDLParser;
use CodeGenerator;

my @idlDirectories;
my $outputDirectory;
my $generator;
my $forceGeneration = 0;

GetOptions('idldir=s@' => \@idlDirectories,
			  'outputdir=s' => \$outputDirectory,
			  'generator=s' => \$generator,
			  'force-generation' => \$forceGeneration);

if (!defined($generator)) {
  die('Must specify generator')
}

if (!@idlDirectories or !defined($outputDirectory)) {
	die('Must specify both idldir and outputdir');
}

my @idlFiles;

sub addIdlFiles {
  my ($dir) = @_;
  my @dirs = ();

  opendir DIR, $dir or die "Can't open directory $dir.\n";
  while (my $file = readdir DIR) {
    next if $file =~ /^\./;
    if (-d "$dir/$file") {
      push @dirs, "$dir/$file";
    } elsif ($file =~ /\.idl$/) {
      push @idlFiles, "$dir/$file";
    }
  }
  closedir DIR;
  
  for my $subdir (@dirs) {
    addIdlFiles($subdir);
  }
}

foreach my $idlDirectory (@idlDirectories) {
  addIdlFiles($idlDirectory);
}

for my $idlPath (@idlFiles) {   
	# Parse the given IDL file.
	my $parser = IDLParser->new(1);
	
	my $document = $parser->Parse($idlPath);
	
	# Generate desired output for given IDL file.
	my $codeGen = CodeGenerator->new(\@idlDirectories, $generator, $outputDirectory);
	$codeGen->ProcessDocument($document, $forceGeneration);
}
