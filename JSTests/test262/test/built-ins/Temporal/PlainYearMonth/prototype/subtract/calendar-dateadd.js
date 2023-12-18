// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: PlainYearMonth.prototype.subtract should call dateAdd with the appropriate values.
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
    if (calls == 2) {
      TemporalHelpers.assertPlainDate(plainDate, 2000, 3, "M03", 31, "plainDate argument");
      TemporalHelpers.assertDuration(duration, 0, -10, 0, 0, 0, 0, 0, 0, 0, 0, "duration argument");
      assert.sameValue(typeof options, "object", "options argument: type");
      assert.sameValue(Object.getPrototypeOf(options), null, "options argument: prototype");
    }
    return super.dateAdd(plainDate, duration, options);
  }
}

const plainYearMonth = new Temporal.PlainYearMonth(2000, 3, new CustomCalendar());
const result = plainYearMonth.subtract({ months: 10 });
TemporalHelpers.assertPlainYearMonth(result, 1999, 5, "M05");
assert.sameValue(calls, 2, "should have called dateAdd 2 times");
