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

# Interesting invariant, which we take advantage of: branching instructions
# always begin with "b", and no non-branching instructions begin with "b".
# Terminal instructions are "jmp" and "ret".

MACRO_INSTRUCTIONS =
    [
     "addi",
     "andi",
     "lshifti",
     "lshiftp",
     "muli",
     "negi",
     "negp",
     "noti",
     "ori",
     "rshifti",
     "urshifti",
     "rshiftp",
     "urshiftp",
     "subi",
     "xori",
     "loadi",
     "loadis",
     "loadb",
     "loadbs",
     "loadh",
     "loadhs",
     "storei",
     "storeb",
     "loadd",
     "moved",
     "stored",
     "addd",
     "divd",
     "subd",
     "muld",
     "sqrtd",
     "ci2d",
     "fii2d", # usage: fii2d <gpr with least significant bits>, <gpr with most significant bits>, <fpr>
     "fd2ii", # usage: fd2ii <fpr>, <gpr with least significant bits>, <gpr with most significant bits>
     "fp2d",
     "fd2p",
     "bdeq",
     "bdneq",
     "bdgt",
     "bdgteq",
     "bdlt",
     "bdlteq",
     "bdequn",
     "bdnequn",
     "bdgtun",
     "bdgtequn",
     "bdltun",
     "bdltequn",
     "btd2i",
     "td2i",
     "bcd2i",
     "movdz",
     "pop",
     "push",
     "move",
     "sxi2p",
     "zxi2p",
     "nop",
     "bieq",
     "bineq",
     "bia",
     "biaeq",
     "bib",
     "bibeq",
     "bigt",
     "bigteq",
     "bilt",
     "bilteq",
     "bbeq",
     "bbneq",
     "bba",
     "bbaeq",
     "bbb",
     "bbbeq",
     "bbgt",
     "bbgteq",
     "bblt",
     "bblteq",
     "btio",
     "btis",
     "btiz",
     "btinz",
     "btbo",
     "btbs",
     "btbz",
     "btbnz",
     "jmp",
     "baddio",
     "baddis",
     "baddiz",
     "baddinz",
     "bsubio",
     "bsubis",
     "bsubiz",
     "bsubinz",
     "bmulio",
     "bmulis",
     "bmuliz",
     "bmulinz",
     "borio",
     "boris",
     "boriz",
     "borinz",
     "break",
     "call",
     "ret",
     "cbeq",
     "cbneq",
     "cba",
     "cbaeq",
     "cbb",
     "cbbeq",
     "cbgt",
     "cbgteq",
     "cblt",
     "cblteq",
     "cieq",
     "cineq",
     "cia",
     "ciaeq",
     "cib",
     "cibeq",
     "cigt",
     "cigteq",
     "cilt",
     "cilteq",
     "tio",
     "tis",
     "tiz",
     "tinz",
     "tbo",
     "tbs",
     "tbz",
     "tbnz",
     "tpo",
     "tps",
     "tpz",
     "tpnz",
     "peek",
     "poke",
     "bpeq",
     "bpneq",
     "bpa",
     "bpaeq",
     "bpb",
     "bpbeq",
     "bpgt",
     "bpgteq",
     "bplt",
     "bplteq",
     "addp",
     "mulp",
     "andp",
     "orp",
     "subp",
     "xorp",
     "loadp",
     "cpeq",
     "cpneq",
     "cpa",
     "cpaeq",
     "cpb",
     "cpbeq",
     "cpgt",
     "cpgteq",
     "cplt",
     "cplteq",
     "storep",
     "btpo",
     "btps",
     "btpz",
     "btpnz",
     "baddpo",
     "baddps",
     "baddpz",
     "baddpnz",
     "bo",
     "bs",
     "bz",
     "bnz",
     "leai",
     "leap",
    ]

X86_INSTRUCTIONS =
    [
     "cdqi",
     "idivi"
    ]

ARMv7_INSTRUCTIONS =
    [
     "smulli",
     "addis",
     "subis",
     "oris"
    ]

INSTRUCTIONS = MACRO_INSTRUCTIONS + X86_INSTRUCTIONS + ARMv7_INSTRUCTIONS

INSTRUCTION_PATTERN = Regexp.new('\\A((' + INSTRUCTIONS.join(')|(') + '))\\Z')

def isBranch(instruction)
    instruction =~ /^b/
end

def hasFallThrough(instruction)
    instruction != "ret" and instruction != "jmp"
end

