#!/usr/bin/perl -w
#
# KDOM IDL parser - main program
#
# Copyright (c) 2005 Nikolas Zimmermann <wildfox@kde.org>
#

use strict;

use Cwd;
use Getopt::Long;

use IDLParser;
use IDLCodeGenerator;

my $generator;
my $outputDir;
my @includeDirs;
my $documentationFile;
my $inputFile;
my $layerOnTop = 0;

# Parse command line parameters.
my $result = GetOptions("generator=s" => \$generator,
						"outputdir=s" => \$outputDir,
						"includedir=s" => \@includeDirs,
						"documentation=s" => \$documentationFile,
						"input=s" => \$inputFile,
						"ontop+" => \$layerOnTop);

if(!defined($generator) or !defined($outputDir) or !defined($documentationFile) or
   (@includeDirs eq 0) or !defined($inputFile))
{
	print "Usage: kdomidl.pl [--generator <string>] [--outputdir <directory>]\n" .
		  "                  [--includedir <directory>]* [--documentation <xml file>]\n" .
		  "                  [--input <idl file>] [--ontop]?\n\n";

	print " Possible values for --generator: 'cpp' / 'js' / 'ecmaInterface'\n";
	print " You can specify multiple --includedir arguments.\n";
	print " In projects using kdom, specify the optional --ontop flag.\n";
	print " Use relative directory paths for --outputdir & --documentation.\n\n";

	exit;
}

# Absolutize the passed include directory paths...
foreach(@includeDirs) {
	$_ = (&cwd) . "/$_" unless $_ =~ /^\//;
}

$outputDir = (&cwd) . "/$outputDir" unless $outputDir =~ /^\//;
$documentationFile = (&cwd) . "/$documentationFile" unless $documentationFile =~ /^\//;
$inputFile = (&cwd) . "/$inputFile" unless $inputFile =~ /^\//;

# Parse the given IDL file.
my $parser = IDLParser->new(0);
my $document = $parser->Parse($inputFile);

# Generate desired output for given IDL file.
my $codeGen = IDLCodeGenerator->new(\@includeDirs, $generator, $outputDir, $documentationFile, $layerOnTop);
$codeGen->ProcessDocument($document);

1;
