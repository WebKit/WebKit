// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const calendar = "IsO8601";

const timeZone = new Temporal.TimeZone("UTC");
const datetime = new Temporal.ZonedDateTime(0n, timeZone);

let arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result1 = Temporal.ZonedDateTime.compare(arg, datetime);
assert.sameValue(result1, 0, "Calendar is case-insensitive (first argument)");
const result2 = Temporal.ZonedDateTime.compare(datetime, arg);
assert.sameValue(result2, 0, "Calendar is case-insensitive (second argument)");

arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar: { calendar } };
const result3 = Temporal.ZonedDateTime.compare(arg, datetime);
assert.sameValue(result3, 0, "Calendar is case-insensitive (nested property, first argument)");
const result4 = Temporal.ZonedDateTime.compare(datetime, arg);
assert.sameValue(result4, 0, "Calendar is case-insensitive (nested property, second argument)");
