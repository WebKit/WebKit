// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  UnbalanceDurationRelative throws a RangeError when duration signs don't match.
features: [Temporal]
---*/

var duration = Temporal.Duration.from({
  years: 1,
  months: 0,
  weeks: 1,
});

var cal = new class extends Temporal.Calendar {
  dateUntil(one, two, options) {
    var result = super.dateUntil(one, two, options);
    return result.negated();
  }
}("iso8601");

var relativeTo = new Temporal.PlainDateTime(1970, 1, 1, 0, 0, 0, 0, 0, 0, cal);
assert.sameValue(relativeTo.getCalendar(), cal);

var options = {
  smallestUnit: "days",
  largestUnit: "month",
  relativeTo,
};

assert.throws(RangeError, () => duration.round(options));
