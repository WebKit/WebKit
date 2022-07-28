// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.subtract
description: PlainDateTime.prototype.subtract should call dateAdd with the appropriate values.
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
    TemporalHelpers.assertPlainDate(plainDate, 2020, 3, "M03", 14, "plainDate argument");
    TemporalHelpers.assertDuration(duration, 0, -10, 0, -1, 0, 0, 0, 0, 0, 0, "duration argument");
    assert.sameValue(typeof options, "object", "options argument: type");
    assert.sameValue(Object.getPrototypeOf(options), null, "options argument: prototype");
    return super.dateAdd(plainDate, duration, options);
  }
}

const plainDateTime = new Temporal.PlainDateTime(2020, 3, 14, 12, 34, 56, 987, 654, 321, new CustomCalendar());
const result = plainDateTime.subtract({ months: 10, hours: 14 });
TemporalHelpers.assertPlainDateTime(result, 2019, 5, "M05", 13, 22, 34, 56, 987, 654, 321);
assert.sameValue(calls, 1, "should have called dateAdd");
