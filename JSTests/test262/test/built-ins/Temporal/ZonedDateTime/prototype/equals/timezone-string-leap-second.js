// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Leap second is a valid ISO string for TimeZone
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1588377600_000_000_000n, new Temporal.TimeZone("UTC"));
let timeZone = "2016-12-31T23:59:60+00:00[UTC]";

assert(instance.equals({ year: 2020, month: 5, day: 2, timeZone }), "leap second is a valid ISO string for TimeZone");
assert(instance.equals({ year: 2020, month: 5, day: 2, timeZone: { timeZone } }), "leap second is a valid ISO String for TimeZone (nested property)");

timeZone = "2021-08-19T17:30:45.123456789+23:59[+23:59:60]";
assert.throws(RangeError, () => instance.equals({ year: 2020, month: 5, day: 2, timeZone }), "leap second in time zone name not valid");
assert.throws(RangeError, () => instance.equals({ year: 2020, month: 5, day: 2, timeZone: { timeZone } }), "leap second in time zone name not valid (nested property)");
