package PQCodePackage;

use strict;
use warnings;

# Function to adapt assembly code from pq-code-package to BoringSSL usage.
# Should be kept as simple as possible, and maybe even become an identity
# function to just mark the origin of an assembly string.
sub asm {
    my ($asm) = @_;

    # BoringSSL does not promise alignment for the key data.
    $asm =~ s/\bvmovdqa\b/vmovdqu/g;

    return $asm;
}

# TODO(crbug.com/33072965): Replace by a common approach.
#
# Adapter to use pq-code-package asm functions (which are red zone less SysV
# ABI) from BoringSSL.
#
# Handles register saving and argument remapping.
#
# Function must not call any non-SysV-ABI functions, and if it calls any
# SysV-ABI functions, $num_xmms must include the xmm registers they clobber
# too.
#
# Arguments:
#   $win64: Whether compiling for Windows.
#   $num_xmms: Number of xmm registers starting with xmm6 that get clobbered.
#   $num_args: Number of arguments the function takes (at most 4).
sub abi_enter {
    my ($win64, $num_xmms, $num_args) = @_;
    my $out = "";
    if ($win64) {
        $out .= "        .seh_startproc\n";
        # Stack layout: xmm6@rsp xmm7 xmm8 ... rdi rsi padding returnaddress.
        # Padding needed to align the xmm registers to 16 bytes boundaries.
        my $stack_alloc = 16 + $num_xmms * 16 + 8;
        $out .= "        subq\t\$$stack_alloc, %rsp\n";
        $out .= "        .seh_stackalloc\t$stack_alloc\n";

        my $rdi_off = $num_xmms * 16;
        my $rsi_off = $rdi_off + 8;
        $out .= "        movq\t%rdi, $rdi_off(%rsp)\n";
        $out .= "        .seh_savereg\t%rdi, $rdi_off\n";
        $out .= "        movq\t%rsi, $rsi_off(%rsp)\n";
        $out .= "        .seh_savereg\t%rsi, $rsi_off\n";

        for (my $i = 0; $i < $num_xmms; $i++) {
            my $reg = 6 + $i;
            my $off = $i * 16;
            $out .= "        vmovdqa\t%xmm$reg, $off(%rsp)\n";
            $out .= "        .seh_savexmm\t%xmm$reg, $off\n";
        }
        $out .= "        .seh_endprologue\n";

        # Map arguments. Funny ordering to spread out data dependencies.
        $out .= "        movq\t%rdx, %rsi\n"
            if $num_args >= 2;
        $out .= "        movq\t%rcx, %rdi\n"
            if $num_args >= 1;
        $out .= "        movq\t%r8, %rdx\n"
            if $num_args >= 3;
        $out .= "        movq\t%r9, %rcx\n"
            if $num_args >= 4;
        die "abi_enter with invalid argument count: got $num_args, want <= 4"
            if $num_args > 4;
    }
    return $out;
}

# TODO(crbug.com/33072965): Replace by a common approach.
#
# Adapter to use pq-code-package asm functions (which are red zone less SysV
# ABI) from BoringSSL.
#
# Handles register restoring and AVX2 zeroing.
#
# Arguments:
#   $win64: Whether compiling for Windows.
#   $num_xmms: Number of xmm registers starting with xmm6 that get clobbered.
#     Must match the argument to `abi_enter`.
sub abi_exit {
    my ($win64, $num_xmms) = @_;
    my $out = "";
    $out .= "        vzeroupper\n";
    if ($win64) {
        my $stack_alloc = 16 + $num_xmms * 16 + 8;

        for (my $i = 0; $i < $num_xmms; $i++) {
            my $reg = 6 + $i;
            my $off = $i * 16;
            $out .= "        vmovdqa\t$off(%rsp), %xmm$reg\n";
        }

        my $rdi_off = $num_xmms * 16;
        my $rsi_off = $rdi_off + 8;
        $out .= "        movq\t$rdi_off(%rsp), %rdi\n";
        $out .= "        movq\t$rsi_off(%rsp), %rsi\n";

        $out .= "        addq\t\$$stack_alloc, %rsp\n";
    }
    $out .= "        ret\n";
    if ($win64) {
        $out .= "        .seh_endproc\n";
    }
    return $out;
}

1;
