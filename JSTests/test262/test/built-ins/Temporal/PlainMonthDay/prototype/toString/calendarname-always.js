// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: If calendarName is "always", the calendar ID should be included.
features: [Temporal]
---*/

const tests = [
  [[], "05-02[u-ca=iso8601]"],
  [[{ toString() { return "custom"; } }], "1972-05-02[u-ca=custom]"],
  [[{ toString() { return "iso8601"; } }], "05-02[u-ca=iso8601]"],
  [[{ toString() { return "ISO8601"; } }], "1972-05-02[u-ca=ISO8601]"],
  [[{ toString() { return "\u0131so8601"; } }], "1972-05-02[u-ca=\u0131so8601]"], // dotless i
];

for (const [args, expected] of tests) {
  const monthday = new Temporal.PlainMonthDay(5, 2, ...args);
  const result = monthday.toString({ calendarName: "always" });
  assert.sameValue(result, expected);
}
