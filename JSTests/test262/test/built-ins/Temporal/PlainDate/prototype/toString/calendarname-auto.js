// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
description: auto value for calendarName option
features: [Temporal]
---*/

const customCalendar = {
  toString() { return "custom"; }
};
const customISOCalendar = {
  toString() { return "iso8601"; }
};
[
  [new Temporal.PlainDate(2000, 5, 2), "2000-05-02"],
  [new Temporal.PlainDate(2000, 5, 2, customCalendar), "2000-05-02[u-ca=custom]"],
  [new Temporal.PlainDate(2000, 5, 2, customISOCalendar), "2000-05-02"],
].forEach(([date, expected]) => {
  const result = date.toString({ calendarName: "auto" });
  assert.sameValue(result, expected, "expected " + expected);
});
