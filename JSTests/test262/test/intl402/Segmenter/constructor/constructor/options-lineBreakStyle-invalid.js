// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter
description: Checks handling of invalid value for the style option to the Segmenter constructor.
info: |
    Intl.Segmenter ([ locales [ , options ]])

    9. Let lineBreakStyle be ? GetOption(options, "lineBreakStyle", "string", « "strict", "normal", "loose" », "normal").
    15. If granularity is "line",
        a. Set segmenter.[[SegmenterLineBreakStyle]] to r.[[lb]].
features: [Intl.Segmenter]
---*/

const invalidOptions = [
  null,
  1,
  "",
  "giant",
  "Strict",
  "STRICT",
  "strict\0",
  "Normal",
  "NORMAL",
  "normal\0",
  "Loose",
  "LOOSE",
  "loose\0",
];

for (const lineBreakStyle of invalidOptions) {
  assert.throws(RangeError, function() {
    new Intl.Segmenter([], { lineBreakStyle });
  }, `${lineBreakStyle} is an invalid style option value`);
}
