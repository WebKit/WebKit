// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: >
  BalanceDuration throws when the result duration is invalid.
info: |
  DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1,
                          y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2,
                          calendar, largestUnit, options )

  ...
  12. Let dateDifference be ? CalendarDateUntil(calendar, date1, date2, untilOptions).
  13. Let balanceResult be ? BalanceDuration(dateDifference.[[Days]], timeDifference.[[Hours]],
      timeDifference.[[Minutes]], timeDifference.[[Seconds]], timeDifference.[[Milliseconds]],
      timeDifference.[[Microseconds]], timeDifference.[[Nanoseconds]], largestUnit).
  ...

  BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds,
                    largestUnit [ , relativeTo ] )

  ...
  3. Else,
    a. Set nanoseconds to ! TotalDurationNanoseconds(days, hours, minutes, seconds, milliseconds,
       microseconds, nanoseconds, 0).
  ...
  15. Return ? CreateTimeDurationRecord(days, hours × sign, minutes × sign, seconds × sign,
      milliseconds × sign, microseconds × sign, nanoseconds × sign).
features: [Temporal]
---*/

var cal = new class extends Temporal.Calendar {
  dateUntil(date1, date2, options) {
    return Temporal.Duration.from({days: Number.MAX_VALUE});
  }
}("iso8601");

var dt1 = new Temporal.PlainDateTime(1970, 1, 1, 0, 0, 0, 0, 0, 0, cal);
var dt2 = new Temporal.PlainDateTime(1970, 1, 1, 0, 0, 0, 0, 0, 0, cal);
var options = {largestUnit: "nanoseconds"};

assert.throws(RangeError, () => dt1.since(dt1, options));
assert.throws(RangeError, () => dt1.since(dt2, options));
assert.throws(RangeError, () => dt2.since(dt1, options));
