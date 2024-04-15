// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: >
    Calendar.monthDayFromFields method validates which fields must be present
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new class extends Temporal.Calendar {
  fields(fields) {
    return fields.slice().concat("fnord");
  }
  monthDayFromFields(fields) {
    // accept everything except fnord
    assert.sameValue(fields.fnord, undefined);
    return new Temporal.PlainMonthDay(1, 1, this, 1972);
  }
}("iso8601");

// This would throw on any non-ISO builtin calendar
const result = Temporal.PlainMonthDay.from({ month: 8, day: 16, calendar });
TemporalHelpers.assertPlainMonthDay(result, "M01", 1, "monthDayFromFields determines what fields are necessary")

assert.throws(
  Test262Error,
  () => Temporal.PlainMonthDay.from({ monthCode: "M09", day: 19, fnord: "fnord", calendar }),
  "monthDayFromFields determines what fields are disallowed"
);
