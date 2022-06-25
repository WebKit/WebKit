// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Temporal.Calendar.prototype.monthDayFromFields will throw TypeError with incorrect input data type.
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

let cal = new Temporal.Calendar("iso8601")

assert.throws(TypeError, () => cal.monthDayFromFields({}), "at least one correctly spelled property is required");
assert.throws(TypeError, () => cal.monthDayFromFields({ monthCode: "M12" }), "day is required with monthCode");
assert.throws(TypeError, () => cal.monthDayFromFields({ year: 2021, month: 12 }), "day is required with year and month");
assert.throws(TypeError, () => cal.monthDayFromFields({ month: 1, day: 17 }), "year is required if month is present");
assert.throws(TypeError, () => cal.monthDayFromFields({ year: 2021, day: 17 }), "either month or monthCode is required");
