// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: BigInt string parsing
esid: pending
features: [BigInt]
---*/

assert.throws(SyntaxError, () => BigInt.parseInt(""));
assert.throws(SyntaxError, () => BigInt.parseInt("@"));
assert.throws(SyntaxError, () => BigInt.parseInt("1", 1));
assert.throws(SyntaxError, () => BigInt.parseInt("1", 37));
assert.sameValue(BigInt.parseInt("0xf", 0), 0xfn);
assert.sameValue(BigInt.parseInt("-0"), 0n);
assert.sameValue(BigInt.parseInt(" 0@"), 0n);
assert.sameValue(BigInt.parseInt("kf12oikf12oikf12oi", 36),
                 5849853453554480289462428370n);
