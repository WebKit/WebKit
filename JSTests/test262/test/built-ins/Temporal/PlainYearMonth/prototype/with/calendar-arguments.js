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

const options = {
  extra: "property",
};
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  yearMonthFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.notSameValue(args[1], options, "args[1] is a copy of options");
    assert.sameValue(args[1].extra, "property", "All properties are copied");
    assert.sameValue(Object.getPrototypeOf(args[1]), null, "Copy has null prototype");
    return super.yearMonthFromFields(...args);
  }
}
const plainYearMonth = new Temporal.PlainYearMonth(2000, 7, new CustomCalendar());
const result = plainYearMonth.with({ month: 5 }, options);
TemporalHelpers.assertPlainYearMonth(result, 2000, 5, "M05");
