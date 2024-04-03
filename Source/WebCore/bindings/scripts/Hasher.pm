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
use bigint;

my $mask64 = 2**64 - 1;
my $mask32 = 2**32 - 1;

sub leftShift($$) {
    my ($value, $distance) = @_;
    return (($value << $distance) & 0xFFFFFFFF);
}

sub avalancheBits($) {
    my ($value) = @_;

    $value &= $mask32;

    # Force "avalanching" of lower 32 bits
    $value ^= leftShift($value, 3);
    $value += ($value >> 5);
    $value = ($value & $mask32);
    $value ^= (leftShift($value, 2) & $mask32);
    $value += ($value >> 15);
    $value = $value & $mask32;
    $value ^= (leftShift($value, 10) & $mask32);

    return $value;
}

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

# Paul Hsieh's SuperFastHash
# http://www.azillionmonkeys.com/qed/hash.html
sub superFastHash {
    my @chars = @_;

    # This hash is designed to work on 16-bit chunks at a time. But since the normal case
    # (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    # were 16-bit chunks, which should give matching results

    my $hash = 0x9e3779b9;
    my $l    = scalar @chars; #I wish this was in Ruby --- Maks
    my $rem  = $l & 1;
    $l = $l >> 1;

    my $s = 0;

    # Main loop
    for (; $l > 0; $l--) {
        $hash   += ord($chars[$s]);
        my $tmp = leftShift(ord($chars[$s+1]), 11) ^ $hash;
        $hash   = (leftShift($hash, 16) & $mask32) ^ $tmp;
        $s += 2;
        $hash += $hash >> 11;
        $hash &= $mask32;
    }

    # Handle end case
    if ($rem != 0) {
        $hash += ord($chars[$s]);
        $hash ^= (leftShift($hash, 11) & $mask32);
        $hash += $hash >> 17;
    }

    $hash = avalancheBits($hash);
    return maskTop8BitsAndAvoidZero($hash);
}

sub uint64_add($$) {
    my ($a, $b) = @_;
    my $sum = $a + $b;
    return $sum & $mask64;
}

sub uint64_multi($$) {
    my ($a, $b) = @_;
    my $product = $a * $b;
    return $product & $mask64;
}

sub wymum($$) {
    my ($A, $B) = @_;

    my $ha = $A >> 32;
    my $hb = $B >> 32;
    my $la = $A & $mask32;
    my $lb = $B & $mask32;
    my $hi;
    my $lo;
    my $rh = uint64_multi($ha, $hb);
    my $rm0 = uint64_multi($ha, $lb);
    my $rm1 = uint64_multi($hb, $la);
    my $rl = uint64_multi($la, $lb);
    my $t = uint64_add($rl, ($rm0 << 32));
    my $c = int($t < $rl);

    $lo = uint64_add($t, ($rm1 << 32));
    $c += int($lo < $t);
    $hi = uint64_add($rh, uint64_add(($rm0 >> 32), uint64_add(($rm1 >> 32), $c)));

    return ($lo, $hi);
};

sub wymix($$) {
    my ($A, $B) = @_;
    ($A, $B) = wymum($A, $B);
    return $A ^ $B;
}

sub convert32BitTo64Bit($) {
    my ($v) = @_;
    my ($mask1) = 281470681808895;   # 0x0000_ffff_0000_ffff
    $v = ($v | ($v << 16)) & $mask1;
    my ($mask2) = 71777214294589695; # 0x00ff_00ff_00ff_00ff
    return ($v | ($v << 8)) & $mask2;
}

sub convert16BitTo32Bit($) {
    my ($v) = @_;
    return ($v | ($v << 8)) & 0x00ff_00ff;
}

sub wyhash {
    # https://github.com/wangyi-fudan/wyhash
    my @chars = @_;
    my $charCount = scalar @chars;
    my $byteCount = $charCount << 1;
    my $charIndex = 0;
    my $seed = 0;
    my @secret = ( 11562461410679940143, 16646288086500911323, 10285213230658275043, 6384245875588680899 );
    my $move1 = (($byteCount >> 3) << 2) >> 1;

    $seed ^= wymix($seed ^ $secret[0], $secret[1]);
    my $a = 0;
    my $b = 0;

    local *c2i = sub {
        my ($i) = @_;
        return ord($chars[$i]);
    };

    local *wyr8 = sub {
        my ($i) = @_;
        my $v = c2i($i) | (c2i($i + 1) << 8) | (c2i($i + 2) << 16) | (c2i($i + 3) << 24);
        return convert32BitTo64Bit($v);
    };

    local *wyr4 = sub {
        my ($i) = @_;
        my $v = c2i($i) | (c2i($i + 1) << 8);
        return convert16BitTo32Bit($v);
    };

    local *wyr2 = sub {
        my ($i) = @_;
        return c2i($i) << 16;
    };

    if ($byteCount <= 16) {
        if ($byteCount >= 4) {
            $a = (wyr4($charIndex) << 32) | wyr4($charIndex + $move1);
            $charIndex = $charIndex + $charCount - 2;
            $b = (wyr4($charIndex) << 32) | wyr4($charIndex - $move1);
        } elsif ($byteCount > 0) {
            $a = wyr2($charIndex);
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
                $seed = wymix(wyr8($charIndex) ^ $secret[1], wyr8($charIndex + 4) ^ $seed);
                $see1 = wymix(wyr8($charIndex + 8) ^ $secret[2], wyr8($charIndex + 12) ^ $see1);
                $see2 = wymix(wyr8($charIndex + 16) ^ $secret[3], wyr8($charIndex + 20) ^ $see2);
                $charIndex += 24;
                $i -= 48;
            } while ($i > 48);
            $seed ^= $see1 ^ $see2;
        }
        while ($i > 16) {
            $seed = wymix(wyr8($charIndex) ^ $secret[1], wyr8($charIndex + 4) ^ $seed);
            $i -= 16;
            $charIndex += 8;
        }
        my $move2 = $i >> 1;
        $a = wyr8($charIndex + $move2 - 8);
        $b = wyr8($charIndex + $move2 - 4);
    }
    $a ^= $secret[1];
    $b ^= $seed;

    ($a, $b) = wymum($a, $b);
    my $hash = wymix($a ^ $secret[0] ^ $byteCount, $b ^ $secret[1]) & $mask32;

    return maskTop8BitsAndAvoidZero($hash);
}


sub GenerateHashValue($$) {
    my ($string, $useWYHash) = @_;
    my @chars = split(/ */, $string);
    my $charCount = scalar @chars;
    if ($useWYHash) {
        if ($charCount <= 48) {
            return superFastHash(@chars);
        }
        return wyhash(@chars);
    } else {
        return superFastHash(@chars);
    }
}

1;
