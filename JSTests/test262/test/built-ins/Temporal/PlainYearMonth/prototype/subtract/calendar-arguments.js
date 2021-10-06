// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: plainyearmonth.prototype.subtract should respect calendar arguments and pass copied options objects.
info: |
    YearMonthFromFields ( calendar, fields [ , options ] )

    5. Let yearMonth be ? Invoke(calendar, "yearMonthFromFields", « fields, options »).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get overflow",
  "get overflow",
  "get overflow.toString",
  "call overflow.toString",
  "get overflow.toString",
  "call overflow.toString",
];
const options = new Proxy({ overflow: "constrain" }, {
  get(target, key) {
    actual.push(`get ${key}`);
    const result = target[key];
    if (result === undefined) {
      return undefined;
    }
    return TemporalHelpers.toPrimitiveObserver(actual, result, key);
  },
  has(target, key) {
    actual.push(`has ${key}`);
    return key in target;
  },
});

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
const plainYearMonth = new Temporal.PlainYearMonth(2000, 7, new CustomCalendar());
const result = plainYearMonth.subtract({ months: 9 }, options);
TemporalHelpers.assertPlainYearMonth(result, 1999, 10, "M10");
assert.compareArray(actual, expected, "copied options object order of operations");
