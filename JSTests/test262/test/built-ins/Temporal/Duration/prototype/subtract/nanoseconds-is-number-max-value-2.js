// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: >
  BalanceDuration computes on exact mathematical number values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1970, 1, 1);
const zonedDateTime = new Temporal.ZonedDateTime(0n, "UTC", "iso8601");

// Largest temporal unit is "day".
const duration1 = Temporal.Duration.from({nanoseconds: Number.MAX_VALUE});
const duration2 = Temporal.Duration.from({nanoseconds: -Number.MAX_VALUE, days: -1});
const nanos = BigInt(Number.MAX_VALUE) * 2n;

TemporalHelpers.assertDuration(
  duration1.subtract(duration2),
  0, 0, 0,
  1 + Number((nanos / (24n * 60n * 60n * 1_000_000_000n))),
  Number((nanos / (60n * 60n * 1_000_000_000n)) % 24n),
  Number((nanos / (60n * 1_000_000_000n)) % 60n),
  Number((nanos / 1_000_000_000n) % 60n),
  Number((nanos / 1_000_000n) % 1000n),
  Number((nanos / 1000n) % 1000n),
  Number(nanos % 1000n),
  "duration1.subtract(duration2)"
);

TemporalHelpers.assertDuration(
  duration1.subtract(duration2, {relativeTo: plainDate}),
  0, 0, 0,
  1 + Number((nanos / (24n * 60n * 60n * 1_000_000_000n))),
  Number((nanos / (60n * 60n * 1_000_000_000n)) % 24n),
  Number((nanos / (60n * 1_000_000_000n)) % 60n),
  Number((nanos / 1_000_000_000n) % 60n),
  Number((nanos / 1_000_000n) % 1000n),
  Number((nanos / 1000n) % 1000n),
  Number(nanos % 1000n),
  "duration1.subtract(duration2, {relativeTo: plainDate})"
);

// Throws a RangeError because the intermediate instant is too large.
assert.throws(RangeError, () => {
  duration1.subtract(duration2, {relativeTo: zonedDateTime});
});
