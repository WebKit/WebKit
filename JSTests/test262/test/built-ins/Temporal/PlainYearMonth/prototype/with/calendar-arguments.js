// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: Correct options value is passed to calendar method
info: |
    YearMonthFromFields ( calendar, fields [ , options ] )

    5. Let yearMonth be ? Invoke(calendar, "yearMonthFromFields", « fields, options »).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const options = {};
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  yearMonthFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.sameValue(args[1], options, "args[1]");
    return super.yearMonthFromFields(...args);
  }
}
const plainYearMonth = new Temporal.PlainYearMonth(2000, 7, new CustomCalendar());
const result = plainYearMonth.with({ month: 5 }, options);
TemporalHelpers.assertPlainYearMonth(result, 2000, 5, "M05");
