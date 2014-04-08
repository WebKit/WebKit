#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use File::Copy qw/move/;
use File::Temp qw/tempfile/;

our $inputScriptFilename;
our $outputScriptFilename;

GetOptions('input-script=s' => \$inputScriptFilename,
           'output-script=s' => \$outputScriptFilename);

unless (defined $inputScriptFilename and defined $outputScriptFilename) {
    print "Usage: $0 --input-script <path> --output-script <path>\n";
    exit;
}

open IN, $inputScriptFilename or die "Couldn't open $inputScriptFilename: $!";
our ($out, $tempFilename) = tempfile(UNLINK => 0) or die;

our $previousLine = "";
while (<IN>) {
    # Warn about console.assert in control flow statement without braces. Can change logic when stripped.
    if (/console\.assert/) {
        if ($previousLine =~ /^\s*(for|if|else|while|do)\b/ && $previousLine !~ /\{\s*$/) {
            print "WARNING: console.assert inside control flow statement without braces on line: $.: $_";
        }
    }

    s/\s*console\.assert\(.*\);\s*//g;
    print $out $_;
    $previousLine = $_ if $_ !~ /^\s*$/;

    # If console.assert is still on the line, either we missed a semicolon or it is multi-line. These did not get stripped.
    if ($_ =~ /\s*console\.assert\(/) {
        if ($_ =~ /\)\s*$/) {
            print "WARNING: console.assert missing trailing semicolon on line $.: $_" ;
        } else {
            print "WARNING: Multi-line console.assert on line $.: $_" ;
        }
    }
}

close $out;
close IN;

move $tempFilename, $outputScriptFilename or die "$!";
