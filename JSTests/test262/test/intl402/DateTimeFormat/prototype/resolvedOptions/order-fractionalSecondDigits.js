// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.DateTimeFormat-fractionalSecondDigits]
---*/

const options = new Intl.DateTimeFormat([], {
  "fractionalSecondDigits": 3,
  "minute": "numeric",
  "second": "numeric",
}).resolvedOptions();

const expected = [
  "locale",
  "calendar",
  "numberingSystem",
  "timeZone",
  "minute",
  "second",
  "fractionalSecondDigits",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
