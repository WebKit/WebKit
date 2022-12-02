// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Leap second is a valid ISO string for TimeZone
features: [Temporal]
---*/

let timeZone = "2016-12-31T23:59:60+00:00[UTC]";

const result1 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result1.timeZone.id, "UTC", "leap second is a valid ISO string for TimeZone");
const result2 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone: { timeZone } });
assert.sameValue(result2.timeZone.id, "UTC", "leap second is a valid ISO string for TimeZone (nested property)");

timeZone = "2021-08-19T17:30:45.123456789+23:59[+23:59:60]";
assert.throws(RangeError, () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone }), "leap second in time zone name not valid");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone: { timeZone } }), "leap second in time zone name not valid (nested property)");
