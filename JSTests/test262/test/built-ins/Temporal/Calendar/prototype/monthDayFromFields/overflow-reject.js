// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Throw RangeError for input data out of range with overflow reject
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISOMonthDayFromFields(fields, options).
  7. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]], calendar, result.[[ReferenceISOYear]]).
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

[-1, 0, 13, 9995].forEach((month) => {
  assert.throws(
    RangeError,
    () => cal.monthDayFromFields({year: 2021, month, day: 5}, { overflow: "reject" }),
    `Month ${month} is out of range for 2021 with overflow: reject`
  );
});

[-1, 0, 32, 999].forEach((day) => {
  assert.throws(
    RangeError,
    () => cal.monthDayFromFields({ year: 2021, month: 12, day }, { overflow: "reject" }),
    `Day ${day} is out of range for 2021-12 with overflow: reject`
  );
  assert.throws(
    RangeError,
    () => cal.monthDayFromFields({ monthCode: "M12", day }, { overflow: "reject" }),
    `Day ${day} is out of range for 2021-M12 with overflow: reject`
  );
});

assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M01", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M01");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M02", day: 30 }, { overflow: "reject" }), "Day 30 is out of range for monthCode M02");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M03", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M03");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M04", day: 31 }, { overflow: "reject" }), "Day 31 is out of range for monthCode M04");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M05", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M05");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M06", day: 31 }, { overflow: "reject" }), "Day 31 is out of range for monthCode M06");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M07", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M07");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M08", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M08");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M09", day: 31 }, { overflow: "reject" }), "Day 31 is out of range for monthCode M09");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M10", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M10");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M11", day: 31 }, { overflow: "reject" }), "Day 31 is out of range for monthCode M11");
assert.throws(RangeError, () => cal.monthDayFromFields(
    { monthCode: "M12", day: 32 }, { overflow: "reject" }), "Day 32 is out of range for monthCode M12");
