// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Temporal.Calendar.prototype.yearMonthFromFields will throw TypeError with incorrect input data type.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISOYearMonthFromFields(fields, options).
  7. Return ? CreateTemporalYearMonth(result.[[Year]], result.[[Month]], calendar, result.[[ReferenceISODay]]).
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601")

assert.throws(TypeError, () => cal.yearMonthFromFields({}), "at least one correctly spelled property is required");
assert.throws(TypeError, () => cal.yearMonthFromFields({ month: 1 }), "year is required");
assert.throws(TypeError, () => cal.yearMonthFromFields({ year: 2021 }), "month or monthCode is required");
