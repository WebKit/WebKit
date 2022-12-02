// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.dayofweek
description: Custom calendar tests for dayOfWeek().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dayOfWeek(...args) {
    ++calls;
    assert.compareArray(args.map(String), [instance].map((arg) => arg.toPlainDateTime().toString()), "dayOfWeek arguments");
    return 7;
  }
}

const calendar = new CustomCalendar();
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
const result = instance.dayOfWeek;
assert.sameValue(result, 7, "result");
assert.sameValue(calls, 1, "calls");
