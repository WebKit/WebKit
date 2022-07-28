// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: PlainYearMonth.prototype.add should call dateAdd with the appropriate values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(plainDate, duration, options) {
    ++calls;
    TemporalHelpers.assertPlainDate(plainDate, 2000, 3, "M03", 1, "plainDate argument");
    TemporalHelpers.assertDuration(duration, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, "duration argument");
    assert.sameValue(typeof options, "object", "options argument: type");
    assert.sameValue(Object.getPrototypeOf(options), null, "options argument: prototype");
    return super.dateAdd(plainDate, duration, options);
  }
}

const plainYearMonth = new Temporal.PlainYearMonth(2000, 3, new CustomCalendar());
const result = plainYearMonth.add({ months: 10 });
TemporalHelpers.assertPlainYearMonth(result, 2001, 1, "M01");
assert.sameValue(calls, 1, "should have called dateAdd");
