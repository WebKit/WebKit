#!/usr/bin/perl -w

# This script is a temporary hack.
# Files are generated in the source directory, when they really should go
# to the DerivedSources directory.
# This should also eventually be a build rule driven off of .idl files
# however a build rule only solution is blocked by several radars:
# <rdar://problems/4251781&4251785>

use File::Path;
use Getopt::Long;

my $namespace = "kdom";
my $baseDirectory = ".";

GetOptions('namespace=s' => \$namespace,
           'basedirectory=s' => \$baseDirectory);

my $kdomBindingsDirectory = "$baseDirectory/kdom/bindings";
my $bindingsDirectory = "$baseDirectory/$namespace/bindings";
my $idlDirectory = "$bindingsDirectory/idl";
my $outputDirectory = "$bindingsDirectory/js";

my @idlFiles;
push @idlFiles, map { chomp; $_ } `find $idlDirectory -name '*.idl' -print | grep -v defs`;

for my $idlPath (@idlFiles) {
    my ($module, $filename) = ($idlPath =~ m|^$idlDirectory/(.*)/(.*)\.idl$|);
    mkpath "$outputDirectory/$module";
    print "$idlPath\n";
    $command = "perl -w";
    $command .= " -I$kdomBindingsDirectory";
    $command .= " -I$bindingsDirectory" if !($namespace eq "kdom");
    $command .= "   $kdomBindingsDirectory/kdomidl.pl";
    $command .= " --generator js";
    $command .= " --outputdir $outputDirectory";
    $command .= " --input $idlPath";
    $command .= " --includedir $kdomBindingsDirectory/idl";
    $command .= " --includedir $idlDirectory" if !($namespace eq "kdom");
    $command .= " --documentation $idlDirectory/$module/docs-$module.xml";
    $command .= " --ontop" if !($namespace eq "kdom");
    `$command`;
}
