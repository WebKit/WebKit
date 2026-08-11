#!/usr/bin/env perl

use strict;
use warnings;
use Getopt::Long;
use File::Copy qw/move/;
use File::Temp qw/tempfile/;
use File::Spec;

sub removeConsoleAssertsInFile($$);
sub removeConsoleAssertsInDirectory($);

our $inputDirectory;
our $inputScriptFilename;
our $outputScriptFilename;

GetOptions('input-directory=s' => \$inputDirectory,
           'input-script=s' => \$inputScriptFilename,
           'output-script=s' => \$outputScriptFilename);

if (defined $inputScriptFilename and defined $outputScriptFilename) {
    removeConsoleAssertsInFile($inputScriptFilename, $outputScriptFilename);
    exit;
}

if (defined $inputDirectory) {
    removeConsoleAssertsInDirectory($inputDirectory);
    exit;
}

print "Usage: $0 --input-script <path> --output-script <path>\n";
print "Usage: $0 --input-directory <path>\n";
exit;

sub removeConsoleAssertsInFile($$)
{
    my $inputScriptFilename = shift;
    my $outputScriptFilename = shift;

    open IN, $inputScriptFilename or die "Couldn't open $inputScriptFilename: $!";
    my ($out, $tempFilename) = tempfile(UNLINK => 0) or die;

    my $previousLine = "";
    while (<IN>) {
        # Warn about console.assert in control flow statement without braces. Can change logic when stripped.
        if (/console\.assert/) {
            if ($previousLine =~ /^\s*(for|if|else|while|do)\b/ && $previousLine !~ /\{\s*$/) {
                print "WARNING: console.assert inside control flow statement without braces on line: $.: $_";
            }
        }

        s/^\s*console\.assert\(.*\);\s*//g;
        print $out $_;
        $previousLine = $_ if $_ !~ /^\s*$/;

        # If console.assert is still on the line, either we missed a semicolon or it is multi-line. These did not get stripped.
        if ($_ =~ /\s*console\.assert\(/) {
            # Allow "console.assert()" to be used as part of a string.
            next if /^.*?["'`].*?console\.assert/;

            # Allow "console.assert()" to be used as part of a comment.
            next if /^.*?(\/\/|\/\*).*?console\.assert/;

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
}

sub removeConsoleAssertsInDirectory($)
{
    my $inputDirectory = shift;

    opendir(DIR, $inputDirectory) || die "$!";
    my @files = grep { !/^\.{1,2}$/ } readdir (DIR);
    closedir(DIR);

    foreach my $file (@files) {
        next if $file eq '.' or $file eq '..';
        my $path = File::Spec->catdir($inputDirectory, $file);
        if (-d $path) {
            removeConsoleAssertsInDirectory($path);
        } elsif ($file =~ /\.js$/) {
            removeConsoleAssertsInFile($path, $path);
        }
    }
}
