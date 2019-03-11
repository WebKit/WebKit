//@ runBigIntEnabled

// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license below.
//
// The << Software identified by reference to the Ecma Standard* ("Software)">>  is protected by copyright and is being
// made available under the  "BSD License", included below. This Software may be subject to third party rights (rights
// from parties other than Ecma International), including patent rights, and no licenses under such third party rights
// are granted under this license even if the third party concerned is a member of Ecma International.  SEE THE ECMA
// CODE OF CONDUCT IN PATENT MATTERS AVAILABLE AT http://www.ecma-international.org/memento/codeofconduct.htm FOR
// INFORMATION REGARDING THE LICENSING OF PATENT CLAIMS THAT ARE REQUIRED TO IMPLEMENT ECMA INTERNATIONAL STANDARDS*.
//
// Copyright (C) 2012-2013 Ecma International
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
// following conditions are met:
// 1.   Redistributions of source code must retain the above copyright notice, this list of conditions and the following
//      disclaimer.
// 2.   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
// 3.   Neither the name of the authors nor Ecma International may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE ECMA INTERNATIONAL "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL ECMA INTERNATIONAL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// * Ecma International Standards hereafter means Ecma International Standards as well as Ecma Technical Reports

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

assert.sameValue(~0n, -1n, "~0n === -1n");
assert.sameValue(~(0n), -1n, "~(0n) === -1n");
assert.sameValue(~1n, -2n, "~1n === -2n");
assert.sameValue(~-1n, 0n, "~-1n === 0n");
assert.sameValue(~(-1n), 0n, "~(-1n) === 0n");
assert.sameValue(~~1n, 1n, "~~1n === 1n");
assert.sameValue(~0x5an, -0x5bn, "~0x5an === -0x5bn");
assert.sameValue(~-0x5an, 0x59n, "~-0x5an === 0x59n");
assert.sameValue(~0xffn, -0x100n, "~0xffn === -0x100n");
assert.sameValue(~-0xffn, 0xfen, "~-0xffn === 0xfen");
assert.sameValue(~0xffffn, -0x10000n, "~0xffffn === -0x10000n");
assert.sameValue(~-0xffffn, 0xfffen, "~-0xffffn === 0xfffen");
assert.sameValue(~0xffffffffn, -0x100000000n, "~0xffffffffn === -0x100000000n");
assert.sameValue(~-0xffffffffn, 0xfffffffen, "~-0xffffffffn === 0xfffffffen");
assert.sameValue(
  ~0xffffffffffffffffn, -0x10000000000000000n,
  "~0xffffffffffffffffn === -0x10000000000000000n");
assert.sameValue(
  ~-0xffffffffffffffffn, 0xfffffffffffffffen,
  "~-0xffffffffffffffffn === 0xfffffffffffffffen");
assert.sameValue(
  ~0x123456789abcdef0fedcba9876543210n, -0x123456789abcdef0fedcba9876543211n,
  "~0x123456789abcdef0fedcba9876543210n === -0x123456789abcdef0fedcba9876543211n");

