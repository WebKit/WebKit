// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: Calendar.dateFromFields method is called with undefined options
features: [Temporal]
---*/

let count = 0;

const calendar = new class extends Temporal.Calendar {
  dateFromFields(fields, options) {
    count++;
    assert.sameValue(options, undefined, "dateFromFields should be called with undefined options");
    return super.dateFromFields(fields, options);
  }
}("iso8601");

const instance = new Temporal.PlainMonthDay(5, 2, calendar);
instance.toPlainDate({ year: 2019 });
assert.sameValue(count, 1, "dateFromFields should have been called on the calendar");
