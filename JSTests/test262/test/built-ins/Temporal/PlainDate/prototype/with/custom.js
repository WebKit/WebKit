// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.with
description: Basic tests with custom calendar
features: [Temporal]
---*/

const result = new Temporal.PlainDate(1920, 5, 3);
const options = {
  extra: "property",
};
let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateFromFields(...args) {
    ++calls;
    assert.sameValue(args.length, 2, "Two arguments");
    assert.sameValue(typeof args[0], "object", "First argument: type");
    assert.sameValue(args[0].day, 18, "First argument: day");
    assert.sameValue(args[0].month, 11, "First argument: month");
    assert.sameValue(args[0].monthCode, "M11", "First argument: monthCode");
    assert.sameValue(args[0].year, 43, "First argument: year");
    assert.notSameValue(args[1], options, "Second argument is a copy of options");
    assert.sameValue(args[1].extra, "property", "All properties are copied");
    assert.sameValue(Object.getPrototypeOf(args[1]), null, "Copy has null prototype");
    return result;
  }
}
const calendar = new CustomCalendar();
const plainDate = new Temporal.PlainDate(1976, 11, 18, calendar);
assert.sameValue(plainDate.with({ year: 43 }, options), result);
assert.sameValue(calls, 1);
