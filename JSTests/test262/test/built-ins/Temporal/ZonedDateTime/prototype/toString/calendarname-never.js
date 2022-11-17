// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
description: If calendarName is "never", the calendar ID should be omitted.
features: [Temporal]
---*/

const tests = [
  [[], "1970-01-01T01:01:01.987654321+00:00[UTC]", "built-in ISO"],
  [[{ toString() { return "custom"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC]", "custom"],
  [[{ toString() { return "iso8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC]", "custom with iso8601 toString"],
  [[{ toString() { return "ISO8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC]", "custom with caps toString"],
  [[{ toString() { return "\u0131so8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC]", "custom with dotless i toString"],
];

for (const [args, expected, description] of tests) {
  const date = new Temporal.ZonedDateTime(3661_987_654_321n, "UTC", ...args);
  const result = date.toString({ calendarName: "never" });
  assert.sameValue(result, expected, `${description} calendar for calendarName = never`);
}
