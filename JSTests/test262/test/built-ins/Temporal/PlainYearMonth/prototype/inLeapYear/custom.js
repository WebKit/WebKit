// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.inleapyear
description: Custom calendar tests for inLeapYear().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  inLeapYear(...args) {
    ++calls;
    assert.compareArray(args, [instance], "inLeapYear arguments");
    return true;
  }
}

const calendar = new CustomCalendar();
const instance = new Temporal.PlainYearMonth(1830, 8, calendar);
const result = instance.inLeapYear;
assert.sameValue(result, true, "result");
assert.sameValue(calls, 1, "calls");
