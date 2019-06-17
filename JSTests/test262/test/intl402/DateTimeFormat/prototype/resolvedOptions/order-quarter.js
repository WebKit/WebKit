// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.DateTimeFormat-quarter]
---*/

const options = new Intl.DateTimeFormat([], {
  "year": "numeric",
  "quarter": "short",
  "month": "numeric",
  "day": "numeric",
}).resolvedOptions();

const expected = [
  "locale",
  "calendar",
  "numberingSystem",
  "timeZone",
  "year",
  "quarter",
  "month",
  "day",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
