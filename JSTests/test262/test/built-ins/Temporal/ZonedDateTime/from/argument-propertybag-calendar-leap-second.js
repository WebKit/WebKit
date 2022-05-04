// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Leap second is a valid ISO string for a calendar in a property bag
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const calendar = "2016-12-31T23:59:60+00:00[UTC]";

let arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result1 = Temporal.ZonedDateTime.from(arg);
assert.sameValue(
  result1.calendar.id,
  "iso8601",
  "leap second is a valid ISO string for calendar"
);

arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar: { calendar } };
const result2 = Temporal.ZonedDateTime.from(arg);
assert.sameValue(
  result2.calendar.id,
  "iso8601",
  "leap second is a valid ISO string for calendar (nested property)"
);
