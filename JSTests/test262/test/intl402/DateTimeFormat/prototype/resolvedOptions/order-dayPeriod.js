// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.DateTimeFormat-dayPeriod]
---*/

const options = new Intl.DateTimeFormat([], {
  "dayPeriod": "short",
  "hour": "numeric",
  "minute": "numeric",
}).resolvedOptions();

const expected = [
  "locale",
  "calendar",
  "numberingSystem",
  "timeZone",
  "hourCycle",
  "hour12",
  "dayPeriod",
  "hour",
  "minute",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
