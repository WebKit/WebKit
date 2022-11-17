// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.daysinmonth
description: Custom calendar tests for daysInMonth().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  daysInMonth(...args) {
    ++calls;
    assert.compareArray(args, [instance], "daysInMonth arguments");
    return 7;
  }
}

const calendar = new CustomCalendar();
const instance = new Temporal.PlainYearMonth(1830, 8, calendar);
const result = instance.daysInMonth;
assert.sameValue(result, 7, "result");
assert.sameValue(calls, 1, "calls");
