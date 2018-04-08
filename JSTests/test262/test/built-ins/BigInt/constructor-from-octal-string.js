// Copyright (C) 2017 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Octal prefixed String should be parsed to BigInt according StringToBigInt
esid: sec-string-to-bigint
info: |
  Apply the algorithm in 3.1.3.1 with the following changes:

  - Replace the StrUnsignedDecimalLiteral production with DecimalDigits
    to not allow decimal points or exponents.

features: [BigInt]
---*/

assert.sameValue(BigInt("0o7"), 7n);
assert.sameValue(BigInt("0o10"), 8n);
assert.sameValue(BigInt("0o20"), 16n);

assert.sameValue(BigInt("0O7"), 7n);
assert.sameValue(BigInt("0O10"), 8n);
assert.sameValue(BigInt("0O20"), 16n);
