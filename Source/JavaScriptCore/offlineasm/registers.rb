# Copyright (C) 2011 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require "config"

GPRS =
    [
     "t0",
     "t1",
     "t2",
     "t3",
     "t4",
     "t5",
     "t6",
     "t7",
     "cfr",
     "a0",
     "a1",
     "a2",
     "a3",
     "a4",
     "a5",
     "a6",
     "a7",
     "r0",
     "r1",
     "sp",
     "lr",
     "pc",
     # 64-bit only registers:
     "csr0",
     "csr1",
     "csr2",
     "csr3",
     "csr4",
     "csr5",
     "csr6",
     "csr7",
     "csr8",
     "csr9",
     "csr10",
     "invalidGPR"
    ]

FPRS =
    [
     "ft0",
     "ft1",
     "ft2",
     "ft3",
     "ft4",
     "ft5",
     "fa0",
     "fa1",
     "fa2",
     "fa3",
     "csfr0",
     "csfr1",
     "csfr2",
     "csfr3",
     "csfr4",
     "csfr5",
     "csfr6",
     "csfr7",
     "csfr8",
     "csfr9",
     "csfr10",
     "csfr11",
     "fr"
    ]

VECS =
    [
     "v0",
     "v0_b",
     "v0_h",
     "v0_i",
     "v0_q",
     "v1",
     "v1_b",
     "v1_h",
     "v1_i",
     "v1_q",
     "v2",
     "v2_b",
     "v2_h",
     "v2_i",
     "v2_q",
     "v3",
     "v3_b",
     "v3_h",
     "v3_i",
     "v3_q",
     "v4",
     "v4_b",
     "v4_h",
     "v4_i",
     "v4_q",
     "v5",
     "v5_b",
     "v5_h",
     "v5_i",
     "v5_q",
     "v6",
     "v6_b",
     "v6_h",
     "v6_i",
     "v6_q",
     "v7",
     "v7_b",
     "v7_h",
     "v7_i",
     "v7_q",
    ]

WASM_GPRS =
    [
     "wa0",
     "wa1",
     "wa2",
     "wa3",
     "wa4",
     "wa5",
     "wa6",
     "wa7",
    ]

WASM_FPRS =
    [
     "wfa0",
     "wfa1",
     "wfa2",
     "wfa3",
     "wfa4",
     "wfa5",
     "wfa6",
     "wfa7",
    ]

WASM_SCRATCHS =
    [
     "ws0",
     "ws1",
     # archtecture specific registers:
     "ws2",
     "ws3",
    ]

REGISTERS = GPRS + FPRS + VECS + WASM_GPRS + WASM_FPRS + WASM_SCRATCHS

GPR_PATTERN = Regexp.new('\\A((' + GPRS.join(')|(') + '))\\Z')
FPR_PATTERN = Regexp.new('\\A((' + FPRS.join(')|(') + '))\\Z')
VEC_PATTERN = Regexp.new('\\A((' + VECS.join(')|(') + '))\\Z')
WASM_GPR_PATTERN = Regexp.new('\\A((' + WASM_GPRS.join(')|(') + '))\\Z')
WASM_FPR_PATTERN = Regexp.new('\\A((' + WASM_FPRS.join(')|(') + '))\\Z')

REGISTER_PATTERN = Regexp.new('\\A((' + REGISTERS.join(')|(') + '))\\Z')
