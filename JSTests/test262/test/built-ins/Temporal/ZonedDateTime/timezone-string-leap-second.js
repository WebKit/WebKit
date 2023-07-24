// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: Leap second is a valid ISO string for TimeZone
features: [Temporal]
---*/

let timeZone = "2016-12-31T23:59:60+00:00[UTC]";

const result = new Temporal.ZonedDateTime(0n, timeZone);
assert.sameValue(result.timeZoneId, "UTC", "Time zone string determined from bracket name");

timeZone = "2021-08-19T17:30:45.123456789+23:59[+23:59:60]";
assert.throws(RangeError, () => new Temporal.ZonedDateTime(0n, timeZone), "leap second in time zone name not valid");
