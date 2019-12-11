// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.formattoparts
description: Checks handling of the compactDisplay option to the NumberFormat constructor.
locale: [ja-JP]
features: [Intl.NumberFormat-unified]
---*/

function verifyFormatParts(actual, expected, message) {
  assert.sameValue(Array.isArray(expected), true, `${message}: expected is Array`);
  assert.sameValue(Array.isArray(actual), true, `${message}: actual is Array`);
  assert.sameValue(actual.length, expected.length, `${message}: length`);

  for (let i = 0; i < actual.length; ++i) {
    assert.sameValue(actual[i].type, expected[i].type, `${message}: parts[${i}].type`);
    assert.sameValue(actual[i].value, expected[i].value, `${message}: parts[${i}].value`);
  }
}

const tests = [
  [
    "auto",
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"987"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"987"}],
  ],
  [
    "always",
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"987"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"plusSign","value":"+"},{"type":"integer","value":"0"}],
    [{"type":"plusSign","value":"+"},{"type":"integer","value":"0"}],
    [{"type":"plusSign","value":"+"},{"type":"integer","value":"987"}],
  ],
  [
    "never",
    [{"type":"integer","value":"987"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"integer","value":"987"}],
  ],
  [
    "exceptZero",
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"987"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"minusSign","value":"-"},{"type":"integer","value":"0"}],
    [{"type":"integer","value":"0"}],
    [{"type":"plusSign","value":"+"},{"type":"integer","value":"0"}],
    [{"type":"plusSign","value":"+"},{"type":"integer","value":"987"}],
  ],
];

for (const [signDisplay, negative, negativeNearZero, negativeZero, zero, positiveNearZero, positive] of tests) {
  const nf = new Intl.NumberFormat("ja-JP", {signDisplay});
  verifyFormatParts(nf.formatToParts(-987), negative);
  verifyFormatParts(nf.formatToParts(-0.0001), negativeNearZero);
  verifyFormatParts(nf.formatToParts(-0), negativeZero);
  verifyFormatParts(nf.formatToParts(0), zero);
  verifyFormatParts(nf.formatToParts(0.0001), positiveNearZero);
  verifyFormatParts(nf.formatToParts(987), positive);
}

