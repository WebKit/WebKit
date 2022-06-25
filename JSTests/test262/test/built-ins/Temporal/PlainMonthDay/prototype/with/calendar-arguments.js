// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.with
description: Correct options value is passed to calendar method
info: |
    MonthDayFromFields ( calendar, fields [ , options ] )

    5. Let monthDay be ? Invoke(calendar, "monthDayFromFields", « fields, options »).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const options = {};
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  monthDayFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.sameValue(args[1], options, "args[1]");
    return super.monthDayFromFields(...args);
  }
}
const plainMonthDay = new Temporal.PlainMonthDay(7, 2, new CustomCalendar());
const result = plainMonthDay.with({ monthCode: "M05" }, options);
TemporalHelpers.assertPlainMonthDay(result, "M05", 2);
