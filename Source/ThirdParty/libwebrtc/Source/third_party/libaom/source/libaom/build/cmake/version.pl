#!/usr/bin/env perl
##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
use strict;
use warnings;
use 5.010;
use Getopt::Long;

my $git_desc = '';
my $version_data;
my $version_filename;
GetOptions('version_data=s' => \$version_data,
           'version_filename=s' => \$version_filename) or
  die("Invalid arg(s): $!");

if (!defined $version_data || length($version_data) == 0 ||
    !defined $version_filename || length($version_filename) == 0) {
  die("--version_data and --version_filename are required.");
}

# Determine if $version_data is a filename or a git tag/description.
my $version_string;
chomp($version_data);
if (-r $version_data) {
  # $version_data is the path to the CHANGELOG. Parse the most recent version.
  my $changelog_filename = $version_data;
  open(my $changelog_file, '<', $changelog_filename) or
    die("Unable to open CHANGELOG @ $changelog_filename: $!.");

  while (my $line = <$changelog_file>) {
    my @split_line = split(" ", $line, 3);
    next if @split_line < 2;
    $version_string = $split_line[1];
    last if substr($version_string, 0, 1) eq "v";
  }
  close($changelog_file);
} else {
  # $version_data is either a tag name or a full git description, one of:
  # tagName OR tagName-commitsSinceTag-shortCommitHash
  # In either case we want the first element of the array returned by split.
  $version_string = (split("-", $version_data))[0];
  $git_desc = $version_data;
}

if (substr($version_string, 0, 1) eq "v") {
  $version_string = substr($version_string, 1);
}

my @version_components = split('\.', $version_string, 4);
my $version_major = $version_components[0];
my $version_minor = $version_components[1];
my $version_patch = $version_components[2];

my $version_extra = "";
if (length($git_desc) > 0) {
  my @git_desc_components = split('-', $git_desc, 2);
  if (@git_desc_components > 1) {
    $version_extra = $git_desc_components[1];
  }
}

open(my $version_file, '>', $version_filename) or
  die("Cannot open $version_filename: $!");

my $version_packed = "((VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | (VERSION_PATCH))";
my $year = (localtime)[5] + 1900;
my $lic_block = << "EOF";
/*
 * Copyright (c) $year, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
EOF

select $version_file;
if (length($git_desc)) {
  print << "EOF";
$lic_block
#define VERSION_MAJOR $version_major
#define VERSION_MINOR $version_minor
#define VERSION_PATCH $version_patch
#define VERSION_EXTRA \"$version_extra\"
#define VERSION_PACKED \\
  $version_packed
#define VERSION_STRING_NOSP \"$git_desc\"
#define VERSION_STRING \" $git_desc\"
EOF
} else {
  print << "EOF";
$lic_block
#define VERSION_MAJOR $version_major
#define VERSION_MINOR $version_minor
#define VERSION_PATCH $version_patch
#define VERSION_EXTRA \"$version_extra\"
#define VERSION_PACKED \\
  $version_packed
#define VERSION_STRING_NOSP \"v$version_string\"
#define VERSION_STRING \" v$version_string\"
EOF
}
close($version_file);
