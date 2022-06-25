// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: compare() does not take the calendar into account
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor(id) {
    super("iso8601");
    this._id = id;
  }
  toString() {
    return this._id;
  }
}

const ym1 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("a"), 1);
const ym2 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("b"), 1);
assert.sameValue(Temporal.PlainYearMonth.compare(ym1, ym2), 0);
