// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
description: always value for calendarName option
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const date1 = new Temporal.PlainDate(2000, 5, 2);
const date2 = new Temporal.PlainDate(2000, 5, 2, calendar);

[
  [date1, "2000-05-02[u-ca=iso8601]"],
  [date2, "2000-05-02[u-ca=custom]"],
].forEach(([date, expected]) => {
  const result = date.toString({ calendarName: "always" });
  assert.sameValue(result, expected, "expected " + expected);
});
