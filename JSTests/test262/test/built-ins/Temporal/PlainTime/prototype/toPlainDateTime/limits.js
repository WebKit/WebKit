// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: Checking limits of representable PlainDateTime
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const midnight = new Temporal.PlainTime(0, 0);
const firstNs = new Temporal.PlainTime(0, 0, 0, 0, 0, 1);
const lastNs = new Temporal.PlainTime(23, 59, 59, 999, 999, 999);
const min = new Temporal.PlainDate(-271821, 4, 19);
const max = new Temporal.PlainDate(275760, 9, 13);

assert.throws(
  RangeError,
  () => midnight.toPlainDateTime(min),
  "Cannot go below representable limit"
);

TemporalHelpers.assertPlainDateTime(
  midnight.toPlainDateTime(max),
  275760, 9, "M09", 13, 0, 0, 0, 0, 0, 0,
  "Midnight of maximum representable PlainDate"
);

TemporalHelpers.assertPlainDateTime(
  firstNs.toPlainDateTime(min),
  -271821, 4, "M04", 19, 0, 0, 0, 0, 0, 1,
  "Computing the minimum (earliest) representable PlainDateTime"
);

TemporalHelpers.assertPlainDateTime(
  lastNs.toPlainDateTime(max),
  275760, 9, "M09", 13, 23, 59, 59, 999, 999, 999,
  "Computing the maximum (latest) representable PlainDateTime"
);
