// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat.prototype.resolvedOptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.ListFormat]
---*/

const options = new Intl.ListFormat().resolvedOptions();

const expected = [
  "locale",
  "type",
  "style",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
