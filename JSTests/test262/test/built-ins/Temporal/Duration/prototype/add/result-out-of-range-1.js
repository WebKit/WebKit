// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: >
  BalanceDuration throws a RangeError when the result is too large.
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1970, 1, 1);
const zonedDateTime = new Temporal.ZonedDateTime(0n, "UTC", "iso8601");

// Largest temporal unit is "second".
const duration = Temporal.Duration.from({seconds: Number.MAX_SAFE_INTEGER});

assert.throws(RangeError, () => {
  duration.add(duration);
});

assert.throws(RangeError, () => {
  duration.add(duration, {relativeTo: plainDate});
});

assert.throws(RangeError, () => {
  duration.add(duration, {relativeTo: zonedDateTime});
});
