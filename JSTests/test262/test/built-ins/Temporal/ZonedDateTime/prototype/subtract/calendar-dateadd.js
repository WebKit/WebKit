// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.subtract
description: ZonedDateTime.prototype.subtract should call dateAdd with the appropriate values.
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
    TemporalHelpers.assertDuration(duration, 0, -10, 0, 0, 0, 0, 0, 0, 0, 0, "duration argument");
    assert.sameValue(typeof options, "object", "options argument: type");
    assert.sameValue(Object.getPrototypeOf(options), null, "options argument: prototype");
    return super.dateAdd(plainDate, duration, options);
  }
}

const zonedDateTime = new Temporal.ZonedDateTime(1584189296_987654321n,
  new Temporal.TimeZone("UTC"), new CustomCalendar());
const result = zonedDateTime.subtract({ months: 10, hours: 14 });
assert.sameValue(result.epochNanoseconds, 1557786896_987654321n);
assert.sameValue(calls, 1, "should have called dateAdd");
