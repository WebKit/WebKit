// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter.prototype.resolvedOptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.Segmenter]
---*/

const options = new Intl.Segmenter([], {
  "granularity": "line",
}).resolvedOptions();

const expected = [
  "locale",
  "granularity",
  "lineBreakStyle",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
