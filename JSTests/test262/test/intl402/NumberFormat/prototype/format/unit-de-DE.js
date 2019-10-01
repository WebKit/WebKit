// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.format
description: Checks handling of the unit style.
locale: [de-DE]
features: [Intl.NumberFormat-unified]
---*/


const tests = [
  [
    -987,
    {
      "short": "-987 m",
      "narrow": "-987 m",
      "long": "-987 Meter",
    }
  ],
  [
    -0.001,
    {
      "short": "-0,001 m",
      "narrow": "-0,001 m",
      "long": "-0,001 Meter",
    }
  ],
  [
    -0,
    {
      "short": "-0 m",
      "narrow": "-0 m",
      "long": "-0 Meter",
    }
  ],
  [
    0,
    {
      "short": "0 m",
      "narrow": "0 m",
      "long": "0 Meter",
    }
  ],
  [
    0.001,
    {
      "short": "0,001 m",
      "narrow": "0,001 m",
      "long": "0,001 Meter",
    }
  ],
  [
    987,
    {
      "short": "987 m",
      "narrow": "987 m",
      "long": "987 Meter",
    }
  ],
];

for (const [number, expectedData] of tests) {
  for (const [unitDisplay, expected] of Object.entries(expectedData)) {
    const nf = new Intl.NumberFormat("de-DE", { style: "unit", unit: "meter", unitDisplay });
    assert.sameValue(nf.format(number), expected);
  }
}

