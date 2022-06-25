// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw RangeError for an out-of-range, conflicting, or ill-formed monthCode
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

const cal = new Temporal.Calendar("iso8601");

["m1", "M1", "m01"].forEach((monthCode) => {
  assert.throws(RangeError, () => cal.yearMonthFromFields({ year: 2021, monthCode }),
    `monthCode '${monthCode}' is not well-formed`);
});

assert.throws(RangeError, () => cal.yearMonthFromFields({ year: 2021, month: 12, monthCode: "M11" }),
     "monthCode and month conflict");

["M00", "M19", "M99", "M13"].forEach((monthCode) => {
  assert.throws(RangeError, () => cal.yearMonthFromFields({ year: 2021, monthCode }),
    `monthCode '${monthCode}' is not valid for year 2021`);
});
