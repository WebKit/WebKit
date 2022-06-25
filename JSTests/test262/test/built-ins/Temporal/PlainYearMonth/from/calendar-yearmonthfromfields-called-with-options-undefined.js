// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: >
    Calendar.yearMonthFromFields method is called with undefined as the options
    value when call originates internally
features: [Temporal]
---*/

const realYearMonthFromFields = Temporal.Calendar.prototype.yearMonthFromFields;
let yearMonthFromFieldsCallCount = 0;
Temporal.Calendar.prototype.yearMonthFromFields = function (fields, options) {
  yearMonthFromFieldsCallCount++;
  assert.sameValue(options, undefined, "yearMonthFromFields shouldn't be called with options");
  return realYearMonthFromFields.call(this, fields, options);
}

Temporal.PlainYearMonth.from("2000-05-01");
assert.sameValue(yearMonthFromFieldsCallCount, 1);

Temporal.Calendar.prototype.yearMonthFromFields = realYearMonthFromFields;
