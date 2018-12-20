// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter
description: Checks handling of valid values for the lineBreakStyle option to the Segmenter constructor.
info: |
    Intl.Segmenter ([ locales [ , options ]])

    9. Let lineBreakStyle be ? GetOption(options, "lineBreakStyle", "string", « "strict", "normal", "loose" », "normal").
    15. If granularity is "line",
        a. Set segmenter.[[SegmenterLineBreakStyle]] to r.[[lb]].
features: [Intl.Segmenter]
---*/

const validOptions = [
  [undefined, "normal"],
  ["strict", "strict"],
  ["normal", "normal"],
  ["loose", "loose"],
  [{ toString() { return "loose"; } }, "loose"],
];

for (const [lineBreakStyle, expected] of validOptions) {
  const segmenter = new Intl.Segmenter([], { granularity: "line", lineBreakStyle });
  const resolvedOptions = segmenter.resolvedOptions();
  assert.sameValue(resolvedOptions.lineBreakStyle, expected);
}
