// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.format
description: Checks handling of the compactDisplay option to the NumberFormat constructor.
locale: [ko-KR]
features: [Intl.NumberFormat-unified]
---*/


const tests = [
  [
    "auto",
    "-987",
    "-0",
    "-0",
    "0",
    "0",
    "987",
  ],
  [
    "always",
    "-987",
    "-0",
    "-0",
    "+0",
    "+0",
    "+987",
  ],
  [
    "never",
    "987",
    "0",
    "0",
    "0",
    "0",
    "987",
  ],
  [
    "exceptZero",
    "-987",
    "-0",
    "-0",
    "0",
    "+0",
    "+987",
  ],
];

for (const [signDisplay, negative, negativeNearZero, negativeZero, zero, positiveNearZero, positive] of tests) {
  const nf = new Intl.NumberFormat("ko-KR", {signDisplay});
  assert.sameValue(nf.format(-987), negative);
  assert.sameValue(nf.format(-0.0001), negativeNearZero);
  assert.sameValue(nf.format(-0), negativeZero);
  assert.sameValue(nf.format(0), zero);
  assert.sameValue(nf.format(0.0001), positiveNearZero);
  assert.sameValue(nf.format(987), positive);
}

