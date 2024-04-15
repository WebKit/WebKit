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
  6. Set fields to ? PrepareTemporalFields(fields, « "day", "month", "monthCode", "year" », « "day" »).
  7. Let overflow be ? ToTemporalOverflow(options).
  8. Perform ? ISOResolveMonth(fields).
  9. Let result be ? ISOMonthDayFromFields(fields, overflow).
  10. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]], "iso8601", result.[[ReferenceISOYear]]).
includes: [temporalHelpers.js]
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

TemporalHelpers.ISOMonths.forEach(({ month, monthCode, daysInMonth }) => {
  const day = daysInMonth + 1;
  assert.throws(RangeError,
    () => cal.monthDayFromFields({ month, day }, { overflow: "reject" }),
    `Day ${day} is out of range for month ${month} with overflow: reject`);
  assert.throws(RangeError,
    () => cal.monthDayFromFields({ monthCode, day }, { overflow: "reject" }),
    `Day ${day} is out of range for monthCode ${monthCode} with overflow: reject`);
});

[ ["month", 2], ["monthCode", "M02"] ].forEach(([ name, value ]) => {
  assert.throws(RangeError,
    () => cal.monthDayFromFields({ year: 2020, [name]: value, day: 30 }, { overflow: "reject" }),
    `Day 30 is out of range for ${name} ${value} in leap year 2020 with overflow: reject`);
  assert.throws(RangeError,
    () => cal.monthDayFromFields({ year: 2021, [name]: value, day: 29 }, { overflow: "reject" }),
    `Day 29 is out of range for ${name} ${value} in common year 2021 with overflow: reject`);
});
