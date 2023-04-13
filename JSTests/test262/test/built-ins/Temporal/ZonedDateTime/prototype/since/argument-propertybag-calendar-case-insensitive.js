// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const calendar = "IsO8601";

let arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result1 = instance.since(arg);
TemporalHelpers.assertDuration(result1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive");

arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar: { calendar } };
const result2 = instance.since(arg);
TemporalHelpers.assertDuration(result2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive (nested property)");
