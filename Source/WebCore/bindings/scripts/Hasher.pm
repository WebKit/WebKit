# Copyright (C) 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# Copyright (C) 2006, 2007 Samuel Weinig <sam@webkit.org>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006-2023 Apple Inc. All rights reserved.
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
# Copyright (C) Research In Motion Limited 2010. All rights reserved.
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
# Copyright (C) 2011 Patrick Gansterer <paroga@webkit.org>
# Copyright (C) 2012 Ericsson AB. All rights reserved.
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

package Hasher;

use strict;
use integer;

# Performance: 'use integer' gives native 64-bit wrapping arithmetic, which is
# vastly faster than the previous 'use bigint' (Math::BigInt arbitrary precision).
# Caveat: '>>' becomes arithmetic (sign-extending) shift under 'use integer',
# so we mask with & $mask32 after >> 32 to get correct unsigned upper-32-bit extraction.

my $mask32 = 0xFFFFFFFF;
my $SIGN_BIT = (1 << 63);
my @secret = ( 11562461410679940143, 16646288086500911323, 10285213230658275043, 6384245875588680899 );

sub maskTop8BitsAndAvoidZero($) {
    my ($value) = @_;

    $value &= $mask32;

    # Save 8 bits for StringImpl to use as flags.
    $value &= 0xffffff;

    # This avoids ever returning a hash code of 0, since that is used to
    # signal "hash not computed yet". Setting the high bit maintains
    # reasonable fidelity to a hash code of 0 because it is likely to yield
    # exactly 0 when hash lookup masks out the high bits.
    $value = (0x80000000 >> 8) if ($value == 0);

    return $value;
}

# Unsigned less-than for 64-bit values under 'use integer' (signed arithmetic).
sub _unsigned_lt($$) {
    return (($_[0] ^ $SIGN_BIT) < ($_[1] ^ $SIGN_BIT)) ? 1 : 0;
}

sub wymum($$) {
    my ($A, $B) = @_;

    my $ha = ($A >> 32) & $mask32;
    my $hb = ($B >> 32) & $mask32;
    my $la = $A & $mask32;
    my $lb = $B & $mask32;
    my $rh = $ha * $hb;
    my $rm0 = $ha * $lb;
    my $rm1 = $hb * $la;
    my $rl = $la * $lb;
    my $t = $rl + ($rm0 << 32);
    my $c = _unsigned_lt($t, $rl);

    my $lo = $t + ($rm1 << 32);
    $c += _unsigned_lt($lo, $t);
    my $hi = $rh + (($rm0 >> 32) & $mask32) + (($rm1 >> 32) & $mask32) + $c;

    return ($lo, $hi);
};

sub wymix($$) {
    my ($A, $B) = @_;
    ($A, $B) = wymum($A, $B);
    return $A ^ $B;
}

# Read 4 characters from string at index $i, convert to 64-bit via convert32BitTo64Bit.
sub _wyr8($$) {
    my ($str, $i) = @_;
    my $v = ord(substr($str, $i, 1)) | (ord(substr($str, $i + 1, 1)) << 8)
          | (ord(substr($str, $i + 2, 1)) << 16) | (ord(substr($str, $i + 3, 1)) << 24);
    # convert32BitTo64Bit
    $v = ($v | ($v << 16)) & 281470681808895;   # 0x0000_ffff_0000_ffff
    return ($v | ($v << 8)) & 71777214294589695; # 0x00ff_00ff_00ff_00ff
}

# Read 2 characters from string at index $i, convert to 32-bit via convert16BitTo32Bit.
sub _wyr4($$) {
    my ($str, $i) = @_;
    my $v = ord(substr($str, $i, 1)) | (ord(substr($str, $i + 1, 1)) << 8);
    # convert16BitTo32Bit
    return ($v | ($v << 8)) & 0x00ff_00ff;
}

sub _wyr2($$) {
    return ord(substr($_[0], $_[1], 1)) << 16;
}

sub GenerateHashValue($) {
    my ($string) = @_;

    # https://github.com/wangyi-fudan/wyhash
    my $charCount = length($string);
    my $byteCount = $charCount << 1;
    my $charIndex = 0;
    my $seed = 0;
    my $move1 = (($byteCount >> 3) << 2) >> 1;

    $seed ^= wymix($seed ^ $secret[0], $secret[1]);
    my $a = 0;
    my $b = 0;

    if ($byteCount <= 16) {
        if ($byteCount >= 4) {
            $a = (_wyr4($string, $charIndex) << 32) | _wyr4($string, $charIndex + $move1);
            $charIndex = $charIndex + $charCount - 2;
            $b = (_wyr4($string, $charIndex) << 32) | _wyr4($string, $charIndex - $move1);
        } elsif ($byteCount > 0) {
            $a = _wyr2($string, $charIndex);
            $b = 0;
        } else {
            $a = $b = 0;
        }
    } else {
        my $i = $byteCount;
        if ($i > 48) {
            my $see1 = $seed;
            my $see2 = $seed;
            do {
                $seed = wymix(_wyr8($string, $charIndex) ^ $secret[1], _wyr8($string, $charIndex + 4) ^ $seed);
                $see1 = wymix(_wyr8($string, $charIndex + 8) ^ $secret[2], _wyr8($string, $charIndex + 12) ^ $see1);
                $see2 = wymix(_wyr8($string, $charIndex + 16) ^ $secret[3], _wyr8($string, $charIndex + 20) ^ $see2);
                $charIndex += 24;
                $i -= 48;
            } while ($i > 48);
            $seed ^= $see1 ^ $see2;
        }
        while ($i > 16) {
            $seed = wymix(_wyr8($string, $charIndex) ^ $secret[1], _wyr8($string, $charIndex + 4) ^ $seed);
            $i -= 16;
            $charIndex += 8;
        }
        my $move2 = $i >> 1;
        $a = _wyr8($string, $charIndex + $move2 - 8);
        $b = _wyr8($string, $charIndex + $move2 - 4);
    }
    $a ^= $secret[1];
    $b ^= $seed;

    ($a, $b) = wymum($a, $b);
    return maskTop8BitsAndAvoidZero(wymix($a ^ $secret[0] ^ $byteCount, $b ^ $secret[1]) & $mask32);
}

1;
