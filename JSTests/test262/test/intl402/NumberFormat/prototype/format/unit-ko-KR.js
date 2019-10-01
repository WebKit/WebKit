// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.format
description: Checks handling of the unit style.
locale: [ko-KR]
features: [Intl.NumberFormat-unified]
---*/


const tests = [
  [
    -987,
    {
      "short": "-987m",
      "narrow": "-987m",
      "long": "-987미터",
    }
  ],
  [
    -0.001,
    {
      "short": "-0.001m",
      "narrow": "-0.001m",
      "long": "-0.001미터",
    }
  ],
  [
    -0,
    {
      "short": "-0m",
      "narrow": "-0m",
      "long": "-0미터",
    }
  ],
  [
    0,
    {
      "short": "0m",
      "narrow": "0m",
      "long": "0미터",
    }
  ],
  [
    0.001,
    {
      "short": "0.001m",
      "narrow": "0.001m",
      "long": "0.001미터",
    }
  ],
  [
    987,
    {
      "short": "987m",
      "narrow": "987m",
      "long": "987미터",
    }
  ],
];

for (const [number, expectedData] of tests) {
  for (const [unitDisplay, expected] of Object.entries(expectedData)) {
    const nf = new Intl.NumberFormat("ko-KR", { style: "unit", unit: "meter", unitDisplay });
    assert.sameValue(nf.format(number), expected);
  }
}

