// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Verify that undefined options are handled correctly.
features: [Temporal]
---*/

// overflow option has no effect on addition in the ISO calendar, so verify this
// with a custom calendar
class CheckedAdd extends Temporal.Calendar {
  constructor() {
    super("iso8601");
    this.called = 0;
  }
  dateAdd(date, duration, options, constructor) {
    this.called += 1;
    if (this.called == 2)
      assert.notSameValue(options, undefined, "options not undefined");
    return super.dateAdd(date, duration, options, constructor);
  }
}
const calendar = new CheckedAdd();

const yearmonth = new Temporal.PlainYearMonth(2000, 3, calendar);
const duration = { months: 1 };

yearmonth.subtract(duration, undefined);
assert.sameValue(calendar.called, 2, "dateAdd should have been called twice");

calendar.called = 0;
yearmonth.subtract(duration);
assert.sameValue(calendar.called, 2, "dateAdd should have been called twice");
