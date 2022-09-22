// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const calendar = "IsO8601";

const timeZone = new Temporal.TimeZone("UTC");
const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result = Temporal.ZonedDateTime.from(arg);
assert.sameValue(result.calendar.id, "iso8601", `Calendar created from string "${calendar}"`);
