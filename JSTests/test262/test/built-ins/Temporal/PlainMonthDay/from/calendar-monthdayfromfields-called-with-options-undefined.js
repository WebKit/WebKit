// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: >
    Calendar.monthDayFromFields method is called with undefined as the options
    value when call originates internally
features: [Temporal]
---*/

const realMonthDayFromFields = Temporal.Calendar.prototype.monthDayFromFields;
let monthDayFromFieldsCallCount = 0;
Temporal.Calendar.prototype.monthDayFromFields = function (fields, options) {
  monthDayFromFieldsCallCount++;
  assert.sameValue(options, undefined, "monthDayFromFields shouldn't be called with options");
  return realMonthDayFromFields.call(this, fields, options);
}

Temporal.PlainMonthDay.from("2000-05-02");
assert.sameValue(monthDayFromFieldsCallCount, 1);

Temporal.Calendar.prototype.monthDayFromFields = realMonthDayFromFields;
