// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: Possibly display calendar when calendarName is "auto"
features: [Temporal]
---*/


const dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23);
const customCal = {
  toString() { return "bogus"; }
};
const fakeISO8601Cal = {
  toString() { return "iso8601"; }
};
const expected = "1976-11-18T15:23:00";

assert.sameValue(dt.toString(), expected, "default is calendar = auto (zero arguments)");
assert.sameValue(dt.toString({ calendarName: "auto" }), expected, "shows only non-ISO calendar if calendarName = auto");

assert.sameValue(
  dt.withCalendar(fakeISO8601Cal).toString({ calendarName: "auto" }),
  expected,
  "Don't show ISO calendar even if calendarName = auto"
);

const dt2 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 0, 0, 0, 0, customCal);

assert.sameValue(
  dt2.toString({ calendarName: "auto" }),
  "1976-11-18T15:23:00[u-ca=bogus]",
  "Don't show calendar if calendarName = auto & PlainDateTime has non-ISO calendar"
);
