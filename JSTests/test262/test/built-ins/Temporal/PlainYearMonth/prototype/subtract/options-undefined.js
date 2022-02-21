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
  }
  dateAdd(date, duration, options, constructor) {
    this.called = true;
    assert.notSameValue(options, undefined, "options not undefined");
    return super.dateAdd(date, duration, options, constructor);
  }
}
const calendar = new CheckedAdd();

const yearmonth = new Temporal.PlainYearMonth(2000, 3, calendar);
const duration = { months: 1 };

yearmonth.subtract(duration, undefined);
yearmonth.subtract(duration);

assert(calendar.called);
