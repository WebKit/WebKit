// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplainyearmonth
description: Correct options value is passed to calendar method
info: |
    YearMonthFromFields ( calendar, fields [ , options ] )

    3. If options is not present, then
        a. Set options to undefined.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  yearMonthFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.sameValue(args[1], undefined, "args[1]");
    return super.yearMonthFromFields(...args);
  }
}
const plainDate = new Temporal.PlainDate(2000, 5, 2, new CustomCalendar());
const result = plainDate.toPlainYearMonth();
TemporalHelpers.assertPlainYearMonth(result, 2000, 5, "M05");
