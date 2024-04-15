// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: >
  RangeError when inconsistent custom calendar method causes mixed signs of
  Duration components
features: [Temporal]
---*/

// Test case provided by André Bargull

const cal = new (class extends Temporal.Calendar {
  dateAdd(date, duration, options) {
    return super.dateAdd(date, duration.negated(), options);
  }
})("iso8601");

const one = new Temporal.PlainDateTime(2000, 1, 1, 0, 0, 0, 0, 0, 0, cal);
const two = new Temporal.PlainDateTime(2020, 5, 10, 12, 12, 0, 0, 0, 0, cal);

assert.throws(RangeError, () => one.until(two, {
  largestUnit: "years",
  smallestUnit: "hours"
}));
