#!/usr/bin/perl -w

use strict;
use FindBin;
use Getopt::Long;

my $python;
GetOptions("python=s" => \$python);

my @command = qw(./planet/planet.py config.ini);
unshift @command, $python if defined($python);

chdir $FindBin::Bin;
exec @command;
