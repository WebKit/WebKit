// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.toplainmonthday
description: Correct options value is passed to calendar method
info: |
    MonthDayFromFields ( calendar, fields [ , options ] )

    3. If options is not present, then
        a. Set options to undefined.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  monthDayFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.sameValue(args[1], undefined, "args[1]");
    return super.monthDayFromFields(...args);
  }
}
const plainDateTime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 123, 456, 789, new CustomCalendar());
const result = plainDateTime.toPlainMonthDay();
TemporalHelpers.assertPlainMonthDay(result, "M05", 2);
