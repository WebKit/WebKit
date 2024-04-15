// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    When adjusting the rounded days after rounding relative to a ZonedDateTime,
    the duration may go out of range
features: [Temporal]
includes: [temporalHelpers.js]
---*/

// Based on a test case by André Bargull

const d = new Temporal.Duration(0, 0, 0, /* days = */ 1, 0, 0, /* s = */ Number.MAX_SAFE_INTEGER - 86400, 0, 0, /* ns = */ 999_999_999);

const timeZone = new Temporal.TimeZone("UTC");
TemporalHelpers.substituteMethod(timeZone, 'getPossibleInstantsFor', [
  TemporalHelpers.SUBSTITUTE_SKIP,
  [new Temporal.Instant(1n)],
]);

const relativeTo = new Temporal.ZonedDateTime(0n, timeZone);

assert.throws(RangeError, () => d.round({
  largestUnit: 'nanoseconds',
  roundingIncrement: 2,
  relativeTo
}));
