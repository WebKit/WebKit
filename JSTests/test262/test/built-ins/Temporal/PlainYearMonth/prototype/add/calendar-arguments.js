// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: PlainYearMonth.prototype.add should respect calendar arguments and pass copied options objects.
info: |
    YearMonthFromFields ( calendar, fields [ , options ] )

    5. Let yearMonth be ? Invoke(calendar, "yearMonthFromFields", « fields, options »).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "ownKeys options",
  "get options.overflow",
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(date, duration, options) {
    const result = super.dateAdd(date, duration, options);
    options.overflow = 'meatloaf';
    return result;
  }
  yearMonthFromFields(...args) {
    assert.sameValue(args.length, 2, "args.length");
    assert.sameValue(typeof args[0], "object", "args[0]");
    assert.notSameValue(args[1], options, "args[1]");
    return super.yearMonthFromFields(...args);
  }
}

const plainYearMonth = new Temporal.PlainYearMonth(2000, 3, new CustomCalendar());
const result = plainYearMonth.add({ months: 10 }, options);
TemporalHelpers.assertPlainYearMonth(result, 2001, 1, "M01");
assert.compareArray(actual, expected, "copied options object order of operations");
