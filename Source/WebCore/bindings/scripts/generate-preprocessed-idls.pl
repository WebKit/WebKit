#!/usr/bin/perl -w
#
# Copyright (C) 2013 Google Inc.  All rights reserved.
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

use preprocessor;
use Getopt::Long;
use Cwd;

my $defines;
my $preprocessor;
my $output;

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'output=s' => \$output);

die('Must specify #define macros using --defines.') unless defined($defines);
die('Must specify the output using --output.') unless defined($output);

my @contents = applyPreprocessor($ARGV[0], $defines, $preprocessor, undef);
open (FILE, ">$output") || die "Couldn't open $output\n";
print FILE @contents;
close FILE;
