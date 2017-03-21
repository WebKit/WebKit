# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::RNG;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
use Bugzilla::Constants qw(ON_WINDOWS);

use Math::Random::ISAAC;
use if ON_WINDOWS, 'Win32::API';

our $RNG;
our @EXPORT_OK = qw(rand srand irand);

# ISAAC, a 32-bit generator, should only be capable of generating numbers
# between 0 and 2^32 - 1. We want _to_float to generate numbers possibly
# including 0, but always less than 1.0. Dividing the integer produced
# by irand() by this number should do that exactly.
use constant DIVIDE_BY => 2**32;

# How many bytes of seed to read.
use constant SEED_SIZE => 16; # 128 bits.

#################
# Windows Stuff #
#################

# The type of cryptographic service provider we want to use.
# This doesn't really matter for our purposes, so we just pick
# PROV_RSA_FULL, which seems reasonable. For more info, see
# http://msdn.microsoft.com/en-us/library/aa380244(v=VS.85).aspx
use constant PROV_RSA_FULL => 1;

# Flags for CryptGenRandom:
# Don't ever display a UI to the user, just fail if one would be needed.
use constant CRYPT_SILENT => 64;
# Don't require existing public/private keypairs.
use constant CRYPT_VERIFYCONTEXT => 0xF0000000;

# For some reason, BOOLEAN doesn't work properly as a return type with 
# Win32::API.
use constant RTLGENRANDOM_PROTO => <<END;
INT SystemFunction036(
  PVOID RandomBuffer,
  ULONG RandomBufferLength
)
END

#################
# RNG Functions #
#################

sub rand (;$) {
    my ($limit) = @_;
    my $int = irand();
    return _to_float($int, $limit);
}

sub irand (;$) {
    my ($limit) = @_;
    Bugzilla::RNG::srand() if !defined $RNG;
    my $int = $RNG->irand();
    if (defined $limit) {
        # We can't just use the mod operator because it will bias
        # our output. Search for "modulo bias" on the Internet for
        # details. This is slower than mod(), but does not have a bias,
        # as demonstrated by Math::Random::Secure's uniform.t test.
        return int(_to_float($int, $limit));
    }
    return $int;
}

sub srand (;$) {
    my ($value) = @_;
    # Remove any RNG that might already have been made.
    $RNG = undef;
    my %args;
    if (defined $value) {
        $args{seed} = $value;
    }
    $RNG = _create_rng(\%args);
}

sub _to_float {
    my ($integer, $limit) = @_;
    $limit ||= 1;
    return ($integer / DIVIDE_BY) * $limit;
}

##########################
# Seed and PRNG Creation #
##########################

sub _create_rng {
    my ($params) = @_;

    if (!defined $params->{seed}) {
        $params->{seed} = _get_seed();
    }

    _check_seed($params->{seed});

    my @seed_ints = unpack('L*', $params->{seed});

    my $rng = Math::Random::ISAAC->new(@seed_ints);

    # It's faster to skip the frontend interface of Math::Random::ISAAC
    # and just use the backend directly. However, in case the internal
    # code of Math::Random::ISAAC changes at some point, we do make sure
    # that the {backend} element actually exists first.
    return $rng->{backend} ? $rng->{backend} : $rng;
}

sub _check_seed {
    my ($seed) = @_;
    if (length($seed) < 8) {
        warn "Your seed is less than 8 bytes (64 bits). It could be"
             . " easy to crack";
    }
    # If it looks like we were seeded with a 32-bit integer, warn the
    # user that they are making a dangerous, easily-crackable mistake.
    elsif (length($seed) <= 10 and $seed =~ /^\d+$/) {
        warn "RNG seeded with a 32-bit integer, this is easy to crack";
    }
}

sub _get_seed {
    return _windows_seed() if ON_WINDOWS;

    if (-r '/dev/urandom') {
        return _read_seed_from('/dev/urandom');
    }

    return _read_seed_from('/dev/random');
}

sub _read_seed_from {
    my ($from) = @_;

    open(my $fh, '<', $from) or die "$from: $!";
    my $buffer;
    read($fh, $buffer, SEED_SIZE);
    if (length($buffer) < SEED_SIZE) {
        die "Could not read enough seed bytes from $from, got only " 
            . length($buffer);
    }
    close $fh;
    return $buffer;
}

sub _windows_seed {
    my ($major, $minor) = (Win32::GetOSVersion())[1,2];
    if ($major < 5) {
        die "Bugzilla does not support versions of Windows before"
            . " Windows 2000";
    }
    # This means Windows 2000.
    if ($major == 5 and $minor == 0) {
        return _win2k_seed();
    }

    my $rtlgenrand = Win32::API->new('advapi32', RTLGENRANDOM_PROTO);
    if (!defined $rtlgenrand) {
        die "Could not import RtlGenRand: $^E";
    }
    my $buffer = chr(0) x SEED_SIZE;
    my $result = $rtlgenrand->Call($buffer, SEED_SIZE);
    if (!$result) {
        die "RtlGenRand failed: $^E";
    }
    return $buffer;
}

sub _win2k_seed {
    my $crypt_acquire = Win32::API->new(
        "advapi32", 'CryptAcquireContext', 'PPPNN', 'I');
    if (!defined $crypt_acquire) {
        die "Could not import CryptAcquireContext: $^E";
    }

    my $crypt_release = Win32::API->new(
        "advapi32", 'CryptReleaseContext', 'NN', 'I');
    if (!defined $crypt_release) {
        die "Could not import CryptReleaseContext: $^E";
    }

    my $crypt_gen_random = Win32::API->new(
        "advapi32", 'CryptGenRandom', 'NNP', 'I');
    if (!defined $crypt_gen_random) {
        die "Could not import CryptGenRandom: $^E";
    }

    my $context = chr(0) x Win32::API::Type->sizeof('PULONG');
    my $acquire_result = $crypt_acquire->Call(
        $context, 0, 0, PROV_RSA_FULL, CRYPT_SILENT | CRYPT_VERIFYCONTEXT);
    if (!defined $acquire_result) {
        die "CryptAcquireContext failed: $^E";
    }

    my $pack_type = Win32::API::Type::packing('PULONG');
    $context = unpack($pack_type, $context);

    my $buffer = chr(0) x SEED_SIZE;
    my $rand_result = $crypt_gen_random->Call($context, SEED_SIZE, $buffer);
    my $rand_error = $^E;
    # We don't check this if it fails, we don't care.
    $crypt_release->Call($context, 0);
    if (!defined $rand_result) {
        die "CryptGenRandom failed: $rand_error";
    }
    return $buffer;
}

1;

=head1 B<Methods in need of POD>

=over

=item srand

=item rand

=item irand

=back
