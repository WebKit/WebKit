// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tostring
description: Verify that undefined options are handled correctly.
features: [Temporal]
---*/

const tests = [
  [[], "05-02"],
  [[{ toString() { return "custom"; } }], "1972-05-02[u-ca=custom]"],
  [[{ toString() { return "iso8601"; } }], "05-02"],
  [[{ toString() { return "ISO8601"; } }], "1972-05-02[u-ca=ISO8601]"],
  [[{ toString() { return "\u0131so8601"; } }], "1972-05-02[u-ca=\u0131so8601]"], // dotless i
];

for (const [args, expected] of tests) {
  const monthday = new Temporal.PlainMonthDay(5, 2, ...args);
  const explicit = monthday.toString(undefined);
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  const implicit = monthday.toString();
  assert.sameValue(implicit, expected, "default calendarName option is auto");
}
