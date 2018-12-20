// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.relativetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.RelativeTimeFormat]
---*/

const options = new Intl.RelativeTimeFormat().resolvedOptions();

const expected = [
  "locale",
  "style",
  "numeric",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
