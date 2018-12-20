// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter
description: Checks handling of valid values for the granularity option to the Segmenter constructor.
info: |
    Intl.Segmenter ([ locales [ , options ]])

    13. Let granularity be ? GetOption(options, "granularity", "string", « "grapheme", "word", "sentence", "line" », "grapheme").
    14. Set segmenter.[[SegmenterGranularity]] to granularity.
features: [Intl.Segmenter]
---*/

const validOptions = [
  [undefined, "grapheme"],
  ["grapheme", "grapheme"],
  ["word", "word"],
  ["sentence", "sentence"],
  ["line", "line"],
  [{ toString() { return "line"; } }, "line"],
];

for (const [granularity, expected] of validOptions) {
  const segmenter = new Intl.Segmenter([], { granularity });
  const resolvedOptions = segmenter.resolvedOptions();
  assert.sameValue(resolvedOptions.granularity, expected);
}
