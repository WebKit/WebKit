// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.daysinyear
description: Custom calendar tests for daysInYear().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  daysInYear(...args) {
    ++calls;
    assert.compareArray(args.map(String), [instance].map((arg) => arg.toPlainDateTime().toString()), "daysInYear arguments");
    return 7;
  }
}

const calendar = new CustomCalendar();
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
const result = instance.daysInYear;
assert.sameValue(result, 7, "result");
assert.sameValue(calls, 1, "calls");
