// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.protoype.tostring
description: auto value for calendarName option
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const yearmonth1 = new Temporal.PlainYearMonth(2000, 5);
const yearmonth2 = new Temporal.PlainYearMonth(2000, 5, calendar);

[
  [yearmonth1, "2000-05"],
  [yearmonth2, "2000-05-01[u-ca=custom]"],
].forEach(([yearmonth, expected]) => {
  const result = yearmonth.toString({ calendarName: "auto" });
  assert.sameValue(result, expected, "calendarName is auto");
});
