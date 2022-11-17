// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: >
  Duration components are precise mathematical integers.
info: |
  Temporal.Duration.compare ( one, two [ , options ] )

  ...
  7. If any of one.[[Years]], two.[[Years]], one.[[Months]], two.[[Months]], one.[[Weeks]], or
     two.[[Weeks]] are not 0, then
    a. Let unbalanceResult1 be ? UnbalanceDurationRelative(one.[[Years]], one.[[Months]],
       one.[[Weeks]], one.[[Days]], "day", relativeTo).
  ...
  9. Let ns1 be ! TotalDurationNanoseconds(days1, one.[[Hours]], one.[[Minutes]], one.[[Seconds]],
     one.[[Milliseconds]], one.[[Microseconds]], one.[[Nanoseconds]], shift1).
  10. Let ns2 be ! TotalDurationNanoseconds(days2, two.[[Hours]], two.[[Minutes]], two.[[Seconds]],
      two.[[Milliseconds]], two.[[Microseconds]], two.[[Nanoseconds]], shift2).
  11. If ns1 > ns2, return 1𝔽.
  12. If ns1 < ns2, return -1𝔽.
  13. Return +0𝔽.

  UnbalanceDurationRelative ( years, months, weeks, days, largestUnit, relativeTo )

  ...
  11. Else,
    a. If any of years, months, and weeks are not zero, then
      ...
      iv. Repeat, while weeks ≠ 0,
        1. Let moveResult be ? MoveRelativeDate(calendar, relativeTo, oneWeek).
        2. Set relativeTo to moveResult.[[RelativeTo]].
        3. Set days to days + moveResult.[[Days]].
        4. Set weeks to weeks - sign.
  12. Return ? CreateDateDurationRecord(years, months, weeks, days).
features: [Temporal]
---*/

var one = Temporal.Duration.from({
  days: Number.MAX_SAFE_INTEGER,
  weeks: 3,
});

var two = Temporal.Duration.from({
  days: Number.MAX_SAFE_INTEGER + 3,
  weeks: 0,
});

var cal = new class extends Temporal.Calendar {
  dateAdd(date, duration, options) {
    // Add one day when one week was requested.
    if (duration.toString() === "P1W") {
      return super.dateAdd(date, "P1D", options);
    }

    // Use zero duration to avoid a RangeError during CalculateOffsetShift.
    return super.dateAdd(date, "PT0S", options);
  }
}("iso8601");

var zdt = new Temporal.ZonedDateTime(0n, "UTC", cal);

// |Number.MAX_SAFE_INTEGER + 1 + 1 + 1| is unequal to |Number.MAX_SAFE_INTEGER + 3|
// when the addition is performed using IEEE-754 semantics. But for compare we have
// to ensure exact mathematical computations are performed.

assert.sameValue(Temporal.Duration.compare(one, two, {relativeTo: zdt}), 0);
