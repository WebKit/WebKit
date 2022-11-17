// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: If calendarName is "auto", "iso8601" should be omitted.
features: [Temporal]
---*/

const tests = [
  [[], "1976-11-18T15:23:00", "built-in ISO"],
  [[{ toString() { return "custom"; } }], "1976-11-18T15:23:00[u-ca=custom]", "custom"],
  [[{ toString() { return "iso8601"; } }], "1976-11-18T15:23:00", "custom with iso8601 toString"],
  [[{ toString() { return "ISO8601"; } }], "1976-11-18T15:23:00[u-ca=ISO8601]", "custom with caps toString"],
  [[{ toString() { return "\u0131so8601"; } }], "1976-11-18T15:23:00[u-ca=\u0131so8601]", "custom with dotless i toString"],
];

for (const [args, expected, description] of tests) {
  const date = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 0, 0, 0, 0, ...args);
  const result = date.toString({ calendarName: "auto" });
  assert.sameValue(result, expected, `${description} calendar for calendarName = auto`);
}
