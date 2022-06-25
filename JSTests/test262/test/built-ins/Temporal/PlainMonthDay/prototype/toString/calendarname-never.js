// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: If calendarName is "never", the calendar ID should be omitted.
features: [Temporal]
---*/

const tests = [
  [[], "05-02"],
  [[{ toString() { return "custom"; } }], "1972-05-02"],
  [[{ toString() { return "iso8601"; } }], "05-02"],
  [[{ toString() { return "ISO8601"; } }], "1972-05-02"],
  [[{ toString() { return "\u0131so8601"; } }], "1972-05-02"], // dotless i
];

for (const [args, expected] of tests) {
  const monthday = new Temporal.PlainMonthDay(5, 2, ...args);
  const result = monthday.toString({ calendarName: "never" });
  assert.sameValue(result, expected);
}
