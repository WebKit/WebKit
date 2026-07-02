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
my @secret = ( 3257665815644502181, 10067880064238660809, 5418857496715711651 );

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

sub rapid_mul128($$) {
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

sub rapid_mix($$) {
    my ($A, $B) = @_;
    ($A, $B) = rapid_mul128($A, $B);
    return $A ^ $B;
}

# Read 8 bytes from string at index $i as a little-endian 64-bit value.
sub _read64($$) {
    my ($str, $i) = @_;
    return ord(substr($str, $i, 1))
        | (ord(substr($str, $i + 1, 1)) << 8)
        | (ord(substr($str, $i + 2, 1)) << 16)
        | (ord(substr($str, $i + 3, 1)) << 24)
        | (ord(substr($str, $i + 4, 1)) << 32)
        | (ord(substr($str, $i + 5, 1)) << 40)
        | (ord(substr($str, $i + 6, 1)) << 48)
        | (ord(substr($str, $i + 7, 1)) << 56);
}

# Read 4 bytes from string at index $i as a little-endian 32-bit value.
sub _read32($$) {
    my ($str, $i) = @_;
    return ord(substr($str, $i, 1))
        | (ord(substr($str, $i + 1, 1)) << 8)
        | (ord(substr($str, $i + 2, 1)) << 16)
        | (ord(substr($str, $i + 3, 1)) << 24);
}

# Read 1-3 bytes from string at index $i (length $k) into a 64-bit value.
sub _readSmall($$$) {
    my ($str, $i, $k) = @_;
    return (ord(substr($str, $i, 1)) << 56)
        | (ord(substr($str, $i + ($k >> 1), 1)) << 32)
        | ord(substr($str, $i + $k - 1, 1));
}

sub GenerateHashValue($) {
    my ($string) = @_;

    # https://github.com/Nicoshev/rapidhash
    # Hashes raw ASCII bytes (1 byte per character).
    my $len = length($string);

    my $seed = rapid_mix(0 ^ $secret[0], $secret[1]) ^ $len;
    my $a = 0;
    my $b = 0;

    if ($len <= 16) {
        if ($len >= 4) {
            my $delta = ($len >= 8) ? 4 : 0;
            $a = (_read32($string, 0) << 32) | _read32($string, $len - 4);
            $b = (_read32($string, $delta) << 32) | _read32($string, $len - 4 - $delta);
        } elsif ($len > 0) {
            $a = _readSmall($string, 0, $len);
            $b = 0;
        } else {
            $a = $b = 0;
        }
    } else {
        my $i = $len;
        my $off = 0;
        if ($i > 48) {
            my $see1 = $seed;
            my $see2 = $seed;
            do {
                $seed = rapid_mix(_read64($string, $off) ^ $secret[0], _read64($string, $off + 8) ^ $seed);
                $see1 = rapid_mix(_read64($string, $off + 16) ^ $secret[1], _read64($string, $off + 24) ^ $see1);
                $see2 = rapid_mix(_read64($string, $off + 32) ^ $secret[2], _read64($string, $off + 40) ^ $see2);
                $off += 48;
                $i -= 48;
            } while ($i >= 48);
            $seed ^= $see1 ^ $see2;
        }
        if ($i > 16) {
            $seed = rapid_mix(_read64($string, $off) ^ $secret[2], _read64($string, $off + 8) ^ $seed ^ $secret[1]);
            if ($i > 32) {
                $seed = rapid_mix(_read64($string, $off + 16) ^ $secret[2], _read64($string, $off + 24) ^ $seed);
            }
        }
        $a = _read64($string, $off + $i - 16);
        $b = _read64($string, $off + $i - 8);
    }
    $a ^= $secret[1];
    $b ^= $seed;

    ($a, $b) = rapid_mul128($a, $b);
    return maskTop8BitsAndAvoidZero(rapid_mix($a ^ $secret[0] ^ $len, $b ^ $secret[1]) & $mask32);
}

1;
