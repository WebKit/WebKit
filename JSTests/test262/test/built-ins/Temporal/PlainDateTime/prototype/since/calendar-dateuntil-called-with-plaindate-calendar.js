// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: calendar.dateUntil() is passed PlainDate objects with the receiver's calendar
info: |
    DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit [ , options ] )

    8. Let _date1_ be ? CreateTemporalDate(_balanceResult_.[[Year]], _balanceResult_.[[Month]], _balanceResult_.[[Day]], _calendar_).
    9. Let _date2_ be ? CreateTemporalDate(_y2_, _mon2_, _d2_, _calendar_).
    12. Let _dateDifference_ be ? CalendarDateUntil(_calendar_, _date1_, _date2_, _untilOptions_).
features: [Temporal]
---*/

class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }

  dateUntil(d1, d2) {
    assert.sameValue(d1.calendar, this, "d1.calendar");
    assert.sameValue(d2.calendar, this, "d2.calendar");
    return new Temporal.Duration();
  }
}
const calendar = new Calendar();
const earlier = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
const later = new Temporal.PlainDateTime(2001, 6, 3, 13, 35, 57, 988, 655, 322, calendar);
const result = earlier.since(later);
assert(result instanceof Temporal.Duration, "result");
