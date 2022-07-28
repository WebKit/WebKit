// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Duration.prototype.add should call dateAdd with the appropriate values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calls = 0;
const expected = [
  {
    plainDate: [1920, 5, "M05", 3],
    duration: [2, 0, 0, 4, 0, 0, 0, 0, 0, 0],
  },
  {
    plainDate: [1922, 5, "M05", 7],
    duration: [0, 10, 0, 0, 0, 0, 0, 0, 0, 0],
  },
];
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(plainDate, duration, options) {
    TemporalHelpers.assertPlainDate(plainDate, ...expected[calls].plainDate,
      `plainDate argument ${calls}`);
    TemporalHelpers.assertDuration(duration, ...expected[calls].duration,
      `duration argument ${calls}`);
    assert.sameValue(options, undefined, "options argument");
    ++calls;
    return super.dateAdd(plainDate, duration, options);
  }
}
const relativeTo = new Temporal.PlainDate(1920, 5, 3, new CustomCalendar());
const duration = new Temporal.Duration(2, 0, 0, 4, 2);
const result = duration.add({ months: 10, hours: 14 }, { relativeTo });
TemporalHelpers.assertDuration(result, 2, 10, 0, 4, 16, 0, 0, 0, 0, 0, "result");
assert.sameValue(calls, 2, "should have called dateAdd");
