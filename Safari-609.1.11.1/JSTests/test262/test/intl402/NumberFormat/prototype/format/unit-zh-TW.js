// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.format
description: Checks handling of the unit style.
locale: [zh-TW]
features: [Intl.NumberFormat-unified]
---*/


const tests = [
  [
    -987,
    {
      "short": "-987 公尺",
      "narrow": "-987公尺",
      "long": "-987 公尺",
    }
  ],
  [
    -0.001,
    {
      "short": "-0.001 公尺",
      "narrow": "-0.001公尺",
      "long": "-0.001 公尺",
    }
  ],
  [
    -0,
    {
      "short": "-0 公尺",
      "narrow": "-0公尺",
      "long": "-0 公尺",
    }
  ],
  [
    0,
    {
      "short": "0 公尺",
      "narrow": "0公尺",
      "long": "0 公尺",
    }
  ],
  [
    0.001,
    {
      "short": "0.001 公尺",
      "narrow": "0.001公尺",
      "long": "0.001 公尺",
    }
  ],
  [
    987,
    {
      "short": "987 公尺",
      "narrow": "987公尺",
      "long": "987 公尺",
    }
  ],
];

for (const [number, expectedData] of tests) {
  for (const [unitDisplay, expected] of Object.entries(expectedData)) {
    const nf = new Intl.NumberFormat("zh-TW", { style: "unit", unit: "meter", unitDisplay });
    assert.sameValue(nf.format(number), expected);
  }
}

