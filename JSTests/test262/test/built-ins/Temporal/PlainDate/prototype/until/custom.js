// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Basic tests with custom calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const result = new Temporal.Duration(1, 3, 5, 7, 9);
const options = { largestUnit: "years" };
let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateUntil(...args) {
    ++calls;
    assert.sameValue(args.length, 3, "Three arguments");
    assert.sameValue(args[0], plainDate, "First argument");
    assert.sameValue(args[1], other, "Second argument");
    assert.sameValue(args[2].largestUnit, "year", "Third argument: largestUnit");
    return result;
  }
}
const calendar = new CustomCalendar();
const plainDate = new Temporal.PlainDate(1976, 11, 18, calendar);
const other = new Temporal.PlainDate(2022, 6, 20, calendar);
TemporalHelpers.assertDuration(plainDate.until(other, options),
  1, 3, 5, 7, 0, 0, 0, 0, 0, 0, "result");
assert.sameValue(calls, 1);
