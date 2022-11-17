// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.weekofyear
description: Custom calendar tests for weekOfYear().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  weekOfYear(...args) {
    ++calls;
    assert.compareArray(args, [pd], "weekOfYear arguments");
    return 7;
  }
}

const calendar = new CustomCalendar();
const pd = new Temporal.PlainDate(1830, 8, 25, calendar);
const result = pd.weekOfYear;
assert.sameValue(result, 7, "result");
assert.sameValue(calls, 1, "calls");
