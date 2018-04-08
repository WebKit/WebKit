// Copyright (C) 2017 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Hexdecimal prefixed String should be parsed to BigInt according StringToBigInt
esid: sec-string-to-bigint
info: |
  Apply the algorithm in 3.1.3.1 with the following changes:

  - Replace the StrUnsignedDecimalLiteral production with DecimalDigits
    to not allow decimal points or exponents.

features: [BigInt]
---*/

assert.sameValue(BigInt("0xa"), 10n);
assert.sameValue(BigInt("0xff"), 255n);
assert.sameValue(BigInt("0xfabc"), 64188n);
assert.sameValue(BigInt("0xfffffffffffffffffff"), 75557863725914323419135n);

assert.sameValue(BigInt("0Xa"), 10n);
assert.sameValue(BigInt("0Xff"), 255n);
assert.sameValue(BigInt("0Xfabc"), 64188n);
assert.sameValue(BigInt("0Xfffffffffffffffffff"), 75557863725914323419135n);
