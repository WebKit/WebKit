// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Time zone IDs are valid input for a time zone
features: [Temporal]
---*/

const instance1 = new Temporal.ZonedDateTime(0n, new Temporal.TimeZone("UTC"));
assert(instance1.equals({ year: 1970, month: 1, day: 1, timeZone: "UTC" }), "Time zone created from string 'UTC'");

const instance2 = new Temporal.ZonedDateTime(0n, new Temporal.TimeZone("-01:30"));
assert(instance2.equals({ year: 1969, month: 12, day: 31, hour: 22, minute: 30, timeZone: "-01:30" }), "Time zone created from string '-01:30'");
