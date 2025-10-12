#!/usr/bin/env perl
##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
no strict 'refs';
use warnings;
use Getopt::Long;
Getopt::Long::Configure("auto_help") if $Getopt::Long::VERSION > 2.32;

my %ALL_FUNCS = ();
my @ALL_ARCHS;
my @ALL_FORWARD_DECLS;
my @REQUIRES;

my %opts = ();
my %disabled = ();
my %required = ();

my @argv;
foreach (@ARGV) {
  $disabled{$1} = 1, next if /--disable-(.*)/;
  $required{$1} = 1, next if /--require-(.*)/;
  push @argv, $_;
}

# NB: use GetOptions() instead of GetOptionsFromArray() for compatibility.
@ARGV = @argv;
GetOptions(
  \%opts,
  'arch=s',
  'sym=s',
  'config=s',
);

foreach my $opt (qw/arch config/) {
  if (!defined($opts{$opt})) {
    warn "--$opt is required!\n";
    Getopt::Long::HelpMessage('-exit' => 1);
  }
}

foreach my $defs_file (@ARGV) {
  if (!-f $defs_file) {
    warn "$defs_file: $!\n";
    Getopt::Long::HelpMessage('-exit' => 1);
  }
}

open CONFIG_FILE, $opts{config} or
  die "Error opening config file '$opts{config}': $!\n";

my %config = ();
while (<CONFIG_FILE>) {
  # TODO(aomedia:349428506,349436249,349450845,349455146): remove AOM_ARCH_
  # after armv7 SIGBUS issues are fixed.
  next if !/^#define\s+(?:AOM_ARCH_|CONFIG_|HAVE_)/;
  chomp;
  my @line_components = split /\s/;
  scalar @line_components > 2 or
    die "Invalid input passed to rtcd.pl via $opts{config}.";
  # $line_components[0] = #define
  # $line_components[1] = flag name ({AOM_ARCH,CONFIG,HAVE}_SOMETHING)
  # $line_components[2] = flag value (0 or 1)
  $config{$line_components[1]} = "$line_components[2]" eq "1" ? "yes" : "";
}
close CONFIG_FILE;

#
# Routines for the RTCD DSL to call
#
sub aom_config($) {
  return (defined $config{$_[0]}) ? $config{$_[0]} : "";
}

sub specialize {
  if (@_ <= 1) {
    die "'specialize' must be called with a function name and at least one ",
        "architecture ('C' is implied): \n@_\n";
  }
  my $fn=$_[0];
  shift;
  foreach my $opt (@_) {
    eval "\$${fn}_${opt}=${fn}_${opt}";
  }
}

sub add_proto {
  my $fn = splice(@_, -2, 1);
  my @proto = @_;
  foreach (@proto) { tr/\t/ / }
  $ALL_FUNCS{$fn} = \@proto;
  specialize $fn, "c";
}

sub require {
  foreach my $fn (keys %ALL_FUNCS) {
    foreach my $opt (@_) {
      my $ofn = eval "\$${fn}_${opt}";
      next if !$ofn;

      # if we already have a default, then we can disable it, as we know
      # we can do better.
      my $best = eval "\$${fn}_default";
      if ($best) {
        my $best_ofn = eval "\$${best}";
        if ($best_ofn && "$best_ofn" ne "$ofn") {
          eval "\$${best}_link = 'false'";
        }
      }
      eval "\$${fn}_default=${fn}_${opt}";
      eval "\$${fn}_${opt}_link='true'";
    }
  }
}

sub forward_decls {
  push @ALL_FORWARD_DECLS, @_;
}

#
# Include the user's directives
#
foreach my $f (@ARGV) {
  open FILE, "<", $f or die "cannot open $f: $!\n";
  my $contents = join('', <FILE>);
  close FILE;
  eval $contents or warn "eval failed: $@\n";
}

#
# Process the directives according to the command line
#
sub process_forward_decls() {
  foreach (@ALL_FORWARD_DECLS) {
    $_->();
  }
}

sub determine_indirection {
  aom_config("CONFIG_RUNTIME_CPU_DETECT") eq "yes" or &require(@ALL_ARCHS);
  foreach my $fn (keys %ALL_FUNCS) {
    my $n = "";
    my @val = @{$ALL_FUNCS{$fn}};
    my $args = pop @val;
    my $rtyp = "@val";
    my $dfn = eval "\$${fn}_default";
    $dfn = eval "\$${dfn}";
    foreach my $opt (@_) {
      my $ofn = eval "\$${fn}_${opt}";
      next if !$ofn;
      my $link = eval "\$${fn}_${opt}_link";
      next if $link && $link eq "false";
      $n .= "x";
    }
    if ($n eq "x") {
      eval "\$${fn}_indirect = 'false'";
    } else {
      eval "\$${fn}_indirect = 'true'";
    }
  }
}

sub declare_function_pointers {
  foreach my $fn (sort keys %ALL_FUNCS) {
    my @val = @{$ALL_FUNCS{$fn}};
    my $args = pop @val;
    my $rtyp = "@val";
    my $dfn = eval "\$${fn}_default";
    $dfn = eval "\$${dfn}";
    foreach my $opt (@_) {
      my $ofn = eval "\$${fn}_${opt}";
      next if !$ofn;
      print "$rtyp ${ofn}($args);\n";
    }
    if (eval "\$${fn}_indirect" eq "false") {
      print "#define ${fn} ${dfn}\n";
    } else {
      print "RTCD_EXTERN $rtyp (*${fn})($args);\n";
    }
    print "\n";
  }
}

sub set_function_pointers {
  foreach my $fn (sort keys %ALL_FUNCS) {
    my @val = @{$ALL_FUNCS{$fn}};
    my $args = pop @val;
    my $rtyp = "@val";
    my $dfn = eval "\$${fn}_default";
    $dfn = eval "\$${dfn}";
    if (eval "\$${fn}_indirect" eq "true") {
      print "    $fn = $dfn;\n";
      foreach my $opt (@_) {
        my $ofn = eval "\$${fn}_${opt}";
        next if !$ofn;
        next if "$ofn" eq "$dfn";
        my $link = eval "\$${fn}_${opt}_link";
        next if $link && $link eq "false";
        my $cond = eval "\$have_${opt}";
        print "    if (${cond}) $fn = $ofn;\n"
      }
    }
  }
}

sub filter {
  my @filtered;
  foreach (@_) { push @filtered, $_ unless $disabled{$_}; }
  return @filtered;
}

#
# Helper functions for generating the arch specific RTCD files
#
sub common_top() {
  my $include_guard = uc($opts{sym})."_H_";
  my @time = localtime;
  my $year = $time[5] + 1900;
  print <<EOF;
/*
 * Copyright (c) ${year}, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// This file is generated. Do not edit.
#ifndef ${include_guard}
#define ${include_guard}

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

EOF

process_forward_decls();
print <<EOF;

#ifdef __cplusplus
extern "C" {
#endif

EOF
declare_function_pointers("c", @ALL_ARCHS);

print <<EOF;
void $opts{sym}(void);

EOF
}

sub common_bottom() {
  my $include_guard = uc($opts{sym})."_H_";
  print <<EOF;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ${include_guard}
EOF
}

sub x86() {
  determine_indirection("c", @ALL_ARCHS);

  # Assign the helper variable for each enabled extension
  foreach my $opt (@ALL_ARCHS) {
    my $opt_uc = uc $opt;
    eval "\$have_${opt}=\"flags & HAS_${opt_uc}\"";
  }

  common_top;
  print <<EOF;
#ifdef RTCD_C
#include "aom_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

EOF

  set_function_pointers("c", @ALL_ARCHS);

  print <<EOF;
}
#endif
EOF
  common_bottom;
}

sub arm() {
  determine_indirection("c", @ALL_ARCHS);

  # Assign the helper variable for each enabled extension
  foreach my $opt (@ALL_ARCHS) {
    my $opt_uc = uc $opt;
    eval "\$have_${opt}=\"flags & HAS_${opt_uc}\"";
  }

  common_top;
  print <<EOF;
#include "config/aom_config.h"

#ifdef RTCD_C
#include "aom_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = aom_arm_cpu_caps();

    (void)flags;

EOF

  set_function_pointers("c", @ALL_ARCHS);

  print <<EOF;
}
#endif
EOF
  common_bottom;
}

sub ppc() {
  determine_indirection("c", @ALL_ARCHS);

  # Assign the helper variable for each enabled extension
  foreach my $opt (@ALL_ARCHS) {
    my $opt_uc = uc $opt;
    eval "\$have_${opt}=\"flags & HAS_${opt_uc}\"";
  }

  common_top;

  print <<EOF;
#include "config/aom_config.h"

#ifdef RTCD_C
#include "aom_ports/ppc.h"
static void setup_rtcd_internal(void)
{
  int flags = ppc_simd_caps();

  (void)flags;

EOF

  set_function_pointers("c", @ALL_ARCHS);

  print <<EOF;
}
#endif
EOF
  common_bottom;
}

sub riscv() {
  determine_indirection("c", @ALL_ARCHS);

  # Assign the helper variable for each enabled extension
  foreach my $opt (@ALL_ARCHS) {
    my $opt_uc = uc $opt;
    eval "\$have_${opt}=\"flags & HAS_${opt_uc}\"";
  }

  common_top;
  print <<EOF;
#ifdef RTCD_C
#include "aom_ports/riscv.h"
static void setup_rtcd_internal(void)
{
    int flags = riscv_simd_caps();

    (void)flags;

EOF

  set_function_pointers("c", @ALL_ARCHS);

  print <<EOF;
}
#endif
EOF
  common_bottom;
}

sub unoptimized() {
  determine_indirection "c";
  common_top;
  print <<EOF;
#include "config/aom_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{
EOF

  set_function_pointers "c";

  print <<EOF;
}
#endif
EOF
  common_bottom;
}

# List of architectures in low-to-high preference order.
my @PRIORITY_ARCH = qw/
  c
  mmx sse sse2 sse3 ssse3 sse4_1 sse4_2 avx avx2
  arm_crc32 neon neon_dotprod neon_i8mm sve sve2
  rvv
  vsx
  dspr2 msa
/;
my %PRIORITY_INDEX;
for (my $i = 0; $i < @PRIORITY_ARCH; $i++) {
  $PRIORITY_INDEX{$PRIORITY_ARCH[$i]} = $i;
}

#
# Main Driver
#

&require("c");
&require(sort { $PRIORITY_INDEX{$a} <=> $PRIORITY_INDEX{$b} } keys %required);
if ($opts{arch} eq 'x86') {
  @ALL_ARCHS = filter(qw/mmx sse sse2 sse3 ssse3 sse4_1 sse4_2 avx avx2/);
  x86;
} elsif ($opts{arch} eq 'x86_64') {
  @ALL_ARCHS = filter(qw/mmx sse sse2 sse3 ssse3 sse4_1 sse4_2 avx avx2
                         avx512/);
  if (keys %required == 0) {
    @REQUIRES = filter(qw/mmx sse sse2/);
    &require(@REQUIRES);
  }
  x86;
} elsif ($opts{arch} =~ /armv[78]\w?/) {
  @ALL_ARCHS = filter(qw/neon/);
  arm;
} elsif ($opts{arch} eq 'arm64' ) {
  @ALL_ARCHS = filter(qw/neon arm_crc32 neon_dotprod neon_i8mm sve sve2/);
  if (keys %required == 0) {
    @REQUIRES = filter(qw/neon/);
    &require(@REQUIRES);
  }
  arm;
} elsif ($opts{arch} eq 'ppc') {
  @ALL_ARCHS = filter(qw/vsx/);
  ppc;
} elsif ($opts{arch} eq 'riscv') {
  @ALL_ARCHS = filter(qw/rvv/);
  riscv;
} else {
  unoptimized;
}

__END__

=head1 NAME

rtcd -

=head1 SYNOPSIS

Usage: rtcd.pl [options] FILE

See 'perldoc rtcd.pl' for more details.

=head1 DESCRIPTION

Reads the Run Time CPU Detections definitions from FILE and generates a
C header file on stdout.

=head1 OPTIONS

Options:
  --arch=ARCH       Architecture to generate defs for (required)
  --disable-EXT     Disable support for EXT extensions
  --require-EXT     Require support for EXT extensions
  --sym=SYMBOL      Unique symbol to use for RTCD initialization function
  --config=FILE     Path to file containing C preprocessor directives to parse
