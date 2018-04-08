// Copyright (C) 2017 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: String should be parsed to BigInt according StringToBigInt
esid: sec-string-to-bigint
info: |
  Apply the algorithm in 3.1.3.1 with the following changes:

  - Replace the StrUnsignedDecimalLiteral production with DecimalDigits
    to not allow decimal points or exponents.

features: [BigInt]
---*/

assert.sameValue(BigInt("10"), 10n);
assert.sameValue(BigInt("18446744073709551616"), 18446744073709551616n);
assert.sameValue(BigInt("7"), 7n);
assert.sameValue(BigInt("88"), 88n);
assert.sameValue(BigInt("900"), 900n);

assert.sameValue(BigInt("-10"), -10n);
assert.sameValue(BigInt("-18446744073709551616"), -18446744073709551616n);
assert.sameValue(BigInt("-7"), -7n);
assert.sameValue(BigInt("-88"), -88n);
assert.sameValue(BigInt("-900"), -900n);
