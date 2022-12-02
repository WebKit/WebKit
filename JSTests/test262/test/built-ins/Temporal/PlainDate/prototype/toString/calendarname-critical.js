// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tostring
description: >
  If calendarName is "calendar", the calendar ID should be included and prefixed
  with "!".
features: [Temporal]
---*/

const tests = [
  [[], "2000-05-02[!u-ca=iso8601]", "built-in ISO"],
  [[{ toString() { return "custom"; } }], "2000-05-02[!u-ca=custom]", "custom"],
  [[{ toString() { return "iso8601"; } }], "2000-05-02[!u-ca=iso8601]", "custom with iso8601 toString"],
  [[{ toString() { return "ISO8601"; } }], "2000-05-02[!u-ca=ISO8601]", "custom with caps toString"],
  [[{ toString() { return "\u0131so8601"; } }], "2000-05-02[!u-ca=\u0131so8601]", "custom with dotless i toString"],
];

for (const [args, expected, description] of tests) {
  const date = new Temporal.PlainDate(2000, 5, 2, ...args);
  const result = date.toString({ calendarName: "critical" });
  assert.sameValue(result, expected, `${description} calendar for calendarName = critical`);
}
