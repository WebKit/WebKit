// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
---*/

const options = new Intl.DateTimeFormat([], {
  "hourCycle": "h24",
  "weekday": "short",
  "era": "short",
  "year": "numeric",
  "month": "numeric",
  "day": "numeric",
  "hour": "numeric",
  "minute": "numeric",
  "second": "numeric",
  "timeZoneName": "short",
}).resolvedOptions();

const expected = [
  "locale",
  "calendar",
  "numberingSystem",
  "timeZone",
  "hourCycle",
  "hour12",
  "weekday",
  "era",
  "year",
  "month",
  "day",
  "hour",
  "minute",
  "second",
  "timeZoneName",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);
