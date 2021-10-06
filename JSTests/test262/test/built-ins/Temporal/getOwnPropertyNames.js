// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-objects
description: Temporal has own property names
includes: [arrayContains.js]
features: [Temporal]
---*/

const expected = [
  "Instant",
  "TimeZone",
  "PlainDate",
  "PlainTime",
  "PlainDateTime",
  "ZonedDateTime",
  "PlainYearMonth",
  "PlainMonthDay",
  "Duration",
  "Calendar",
  "Now",
];
const keys = Object.getOwnPropertyNames(Temporal);
assert.sameValue(keys.length, expected.length, "length");
assert(arrayContains(keys, expected), "contents");
