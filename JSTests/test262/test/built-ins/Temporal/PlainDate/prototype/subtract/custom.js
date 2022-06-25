// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: Basic tests with custom calendar
includes: [compareArray.js,temporalHelpers.js]
features: [Temporal]
---*/

const result = new Temporal.PlainDate(1920, 5, 3);
let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(...args) {
    ++calls;
    assert.sameValue(args.length, 3, "Three arguments");
    assert.sameValue(args[0], plainDate, "First argument");
    TemporalHelpers.assertDuration(args[1], -43, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Second argument");
    assert.sameValue(typeof args[2], "object", "Third argument: type");
    assert.sameValue(Object.getPrototypeOf(args[2]), null, "Third argument: prototype");
    assert.compareArray(Object.keys(args[2]), [], "Third argument: keys");
    return result;
  }
}
const calendar = new CustomCalendar();
const plainDate = new Temporal.PlainDate(1976, 11, 18, calendar);
assert.sameValue(plainDate.subtract({ years: 43 }), result);
assert.sameValue(calls, 1);
