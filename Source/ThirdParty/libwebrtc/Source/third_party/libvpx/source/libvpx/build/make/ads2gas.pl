#!/usr/bin/env perl
##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


# ads2gas.pl
# Author: Eric Fung (efung (at) acm.org)
#
# Convert ARM Developer Suite 1.0.1 syntax assembly source to GNU as format
#
# Usage: cat inputfile | perl ads2gas.pl > outputfile
#

use FindBin;
use lib $FindBin::Bin;
use thumb;

my $thumb = 0;
my $elf = 1;

foreach my $arg (@ARGV) {
    $thumb = 1 if ($arg eq "-thumb");
    $elf = 0 if ($arg eq "-noelf");
}

print "@ This file was created from a .asm file\n";
print "@  using the ads2gas.pl script.\n";
print "\t.syntax unified\n";
if ($thumb) {
    print "\t.thumb\n";
}

# Stack of procedure names.
@proc_stack = ();

while (<STDIN>)
{
    undef $comment;
    undef $line;
    $comment_char = ";";
    $comment_sub = "@";

    # Handle comments.
    if (/$comment_char/)
    {
      $comment = "";
      ($line, $comment) = /(.*?)$comment_char(.*)/;
      $_ = $line;
    }

    # Load and store alignment
    s/@/,:/g;

    # Hexadecimal constants prefaced by 0x
    s/#&/#0x/g;

    # Convert :OR: to |
    s/:OR:/ | /g;

    # Convert :AND: to &
    s/:AND:/ & /g;

    # Convert :NOT: to ~
    s/:NOT:/ ~ /g;

    # Convert :SHL: to <<
    s/:SHL:/ << /g;

    # Convert :SHR: to >>
    s/:SHR:/ >> /g;

    # Convert ELSE to .else
    s/\bELSE\b/.else/g;

    # Convert ENDIF to .endif
    s/\bENDIF\b/.endif/g;

    # Convert ELSEIF to .elseif
    s/\bELSEIF\b/.elseif/g;

    # Convert LTORG to .ltorg
    s/\bLTORG\b/.ltorg/g;

    # Convert endfunc to nothing.
    s/\bendfunc\b//ig;

    # Convert FUNCTION to nothing.
    s/\bFUNCTION\b//g;
    s/\bfunction\b//g;

    s/\bENTRY\b//g;
    s/\bMSARMASM\b/0/g;
    s/^\s+end\s+$//g;

    # Convert IF :DEF:to .if
    # gcc doesn't have the ability to do a conditional
    # if defined variable that is set by IF :DEF: on
    # armasm, so convert it to a normal .if and then
    # make sure to define a value elesewhere
    if (s/\bIF :DEF:\b/.if /g)
    {
        s/=/==/g;
    }

    # Convert IF to .if
    if (s/\bIF\b/.if/g)
    {
        s/=+/==/g;
    }

    # Convert INCLUDE to .INCLUDE "file"
    s/INCLUDE(\s*)(.*)$/.include $1\"$2\"/;

    # Code directive (ARM vs Thumb)
    s/CODE([0-9][0-9])/.code $1/;

    # No AREA required
    # But ALIGNs in AREA must be obeyed
    s/^\s*AREA.*ALIGN=([0-9])$/.text\n.p2align $1/;
    # If no ALIGN, strip the AREA and align to 4 bytes
    s/^\s*AREA.*$/.text\n.p2align 2/;

    # DCD to .word
    # This one is for incoming symbols
    s/DCD\s+\|(\w*)\|/.long $1/;

    # DCW to .short
    s/DCW\s+\|(\w*)\|/.short $1/;
    s/DCW(.*)/.short $1/;

    # Constants defined in scope
    s/DCD(.*)/.long $1/;
    s/DCB(.*)/.byte $1/;

    # Make function visible to linker, and make additional symbol with
    # prepended underscore
    if ($elf) {
        s/EXPORT\s+\|([\$\w]*)\|/.global $1 \n\t.type $1, function/;
    } else {
        s/EXPORT\s+\|([\$\w]*)\|/.global $1/;
    }
    s/IMPORT\s+\|([\$\w]*)\|/.global $1/;

    s/EXPORT\s+([\$\w]*)/.global $1/;
    s/export\s+([\$\w]*)/.global $1/;

    # No vertical bars required; make additional symbol with prepended
    # underscore
    s/^\|(\$?\w+)\|/_$1\n\t$1:/g;

    # Labels need trailing colon
#   s/^(\w+)/$1:/ if !/EQU/;
    # put the colon at the end of the line in the macro
    s/^([a-zA-Z_0-9\$]+)/$1:/ if !/EQU/;

    # ALIGN directive
    s/\bALIGN\b/.balign/g;

    if ($thumb) {
        # ARM code - we force everything to thumb with the declaration in the header
        s/\sARM//g;
    } else {
        # ARM code
        s/\sARM/.arm/g;
    }

    # push/pop
    s/(push\s+)(r\d+)/stmdb sp\!, \{$2\}/g;
    s/(pop\s+)(r\d+)/ldmia sp\!, \{$2\}/g;

    # NEON code
    s/(vld1.\d+\s+)(q\d+)/$1\{$2\}/g;
    s/(vtbl.\d+\s+[^,]+),([^,]+)/$1,\{$2\}/g;

    if ($thumb) {
        thumb::FixThumbInstructions($_, 0);
    }

    # eabi_attributes numerical equivalents can be found in the
    # "ARM IHI 0045C" document.

    if ($elf) {
        # REQUIRE8 Stack is required to be 8-byte aligned
        s/\sREQUIRE8/.eabi_attribute 24, 1 \@Tag_ABI_align_needed/g;

        # PRESERVE8 Stack 8-byte align is preserved
        s/\sPRESERVE8/.eabi_attribute 25, 1 \@Tag_ABI_align_preserved/g;
    } else {
        s/\sREQUIRE8//;
        s/\sPRESERVE8//;
    }

    # Use PROC and ENDP to give the symbols a .size directive.
    # This makes them show up properly in debugging tools like gdb and valgrind.
    if (/\bPROC\b/)
    {
        my $proc;
        /^_([\.0-9A-Z_a-z]\w+)\b/;
        $proc = $1;
        push(@proc_stack, $proc) if ($proc);
        s/\bPROC\b/@ $&/;
    }
    if (/\bENDP\b/)
    {
        my $proc;
        s/\bENDP\b/@ $&/;
        $proc = pop(@proc_stack);
        $_ = "\t.size $proc, .-$proc".$_ if ($proc and $elf);
    }

    # EQU directive
    s/(\S+\s+)EQU(\s+\S+)/.equ $1, $2/;

    # Begin macro definition
    if (/\bMACRO\b/) {
        $_ = <STDIN>;
        s/^/.macro/;
        s/\$//g;                # remove formal param reference
        s/;/@/g;                # change comment characters
    }

    # For macros, use \ to reference formal params
    s/\$/\\/g;                  # End macro definition
    s/\bMEND\b/.endm/;              # No need to tell it where to stop assembling
    next if /^\s*END\s*$/;
    print;
    print "$comment_sub$comment\n" if defined $comment;
}

# Mark that this object doesn't need an executable stack.
printf ("\t.section\t.note.GNU-stack,\"\",\%\%progbits\n") if $elf;
