// Copyright (C) 2017 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Empty String should in BigInt should result into 0n
esid: sec-string-to-bigint
info: |
  Apply the algorithm in 3.1.3.1 with the following changes:

  EDITOR'S NOTE StringToBigInt("") is 0n according to the logic in 3.1.3.1.

features: [BigInt]
---*/

assert.sameValue(BigInt(""), 0n);
assert.sameValue(BigInt(" "), 0n);
