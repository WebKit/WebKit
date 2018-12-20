// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter
description: Checks handling of non-object option arguments to the Segmenter constructor.
info: |
    Intl.Segmenter ([ locales [ , options ]])

    5. Else
        a. Let options be ? ToObject(options).
features: [Intl.Segmenter]
---*/

Object.defineProperties(Object.prototype, {
  "lineBreakStyle": {
    value: "loose",
  },
  "granularity": {
    value: "line",
  },
})

const optionsArguments = [
  true,
  "test",
  7,
  Symbol(),
  {},
];

for (const options of optionsArguments) {
  const segmenter = new Intl.Segmenter([], options);
  const resolvedOptions = segmenter.resolvedOptions();
  assert.sameValue(resolvedOptions.lineBreakStyle, "loose",
    `options argument ${String(options)} should yield the correct value for "lineBreakStyle"`);
  assert.sameValue(resolvedOptions.granularity, "line",
    `options argument ${String(options)} should yield the correct value for "granularity"`);
}
