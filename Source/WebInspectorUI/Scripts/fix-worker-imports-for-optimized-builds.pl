#!/usr/bin/env perl

use strict;
use warnings;
use Getopt::Long;
use File::Copy qw/move/;
use File::Temp qw/tempfile/;
use File::Spec;

sub fixWorkerImportsInFile($$);
sub fixWorkerImportsInDirectory($);

our $inputDirectory;

GetOptions('input-directory=s' => \$inputDirectory);

if (defined $inputDirectory) {
    fixWorkerImportsInDirectory($inputDirectory);
    exit;
}

print "Usage: $0 --input-directory <path>\n";
exit;

sub fixWorkerImportsInFile($$)
{
    my $inputScriptFilename = shift;
    my $outputScriptFilename = shift;

    open IN, $inputScriptFilename or die "Couldn't open $inputScriptFilename: $!";
    my ($out, $tempFilename) = tempfile(UNLINK => 0) or die;

    my $previousLine = "";
    while (<IN>) {
        s|/External/Esprima/esprima.js|/Esprima.js|;
        print $out $_;

        # Error if there is an "External/" path that we did not rewrite.
        if ($_ =~ /External\//) {
            my $sanitizedPath = $inputScriptFilename;
            $sanitizedPath =~ s/^.*?Workers/Workers/;
            print "ERROR: $sanitizedPath: Unhandled External importScript in Worker script on line $.: $_";
            exit 1;
        }
    }

    close $out;
    close IN;

    move $tempFilename, $outputScriptFilename or die "$!";
}

sub fixWorkerImportsInDirectory($)
{
    my $inputDirectory = shift;

    opendir(DIR, $inputDirectory) || die "$!";
    my @files = grep { !/^\.{1,2}$/ } readdir (DIR);
    closedir(DIR);

    foreach my $file (@files) {
        next if $file eq '.' or $file eq '..';
        my $path = File::Spec->catdir($inputDirectory, $file);
        if (-d $path) {
            fixWorkerImportsInDirectory($path);
        } elsif ($file =~ /\.js$/) {
            fixWorkerImportsInFile($path, $path);
        }
    }
}
