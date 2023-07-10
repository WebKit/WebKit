// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone }), "bare date-time string is not a time zone");

timeZone = "2021-08-19T17:30Z";
const result1 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result1.timeZoneId, "UTC", "date-time + Z is UTC time zone");

timeZone = "2021-08-19T17:30-07:00";
const result2 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result2.timeZoneId, "-07:00", "date-time + offset is the offset time zone");

timeZone = "2021-08-19T17:30[UTC]";
const result3 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result3.timeZoneId, "UTC", "date-time + IANA annotation is the IANA time zone");

timeZone = "2021-08-19T17:30Z[UTC]";
const result4 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result4.timeZoneId, "UTC", "date-time + Z + IANA annotation is the IANA time zone");

timeZone = "2021-08-19T17:30-07:00[UTC]";
const result5 = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
assert.sameValue(result5.timeZoneId, "UTC", "date-time + offset + IANA annotation is the IANA time zone");
