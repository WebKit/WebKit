// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetimeiso
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => instance.toZonedDateTimeISO(timeZone), "bare date-time string is not a time zone");

timeZone = "2021-08-19T17:30Z";
const result1 = instance.toZonedDateTimeISO(timeZone);
assert.sameValue(result1.timeZoneId, "UTC", "date-time + Z is UTC time zone");

timeZone = "2021-08-19T17:30-07:00";
const result2 = instance.toZonedDateTimeISO(timeZone);
assert.sameValue(result2.timeZoneId, "-07:00", "date-time + offset is the offset time zone");

timeZone = "2021-08-19T17:30[UTC]";
const result3 = instance.toZonedDateTimeISO(timeZone);
assert.sameValue(result3.timeZoneId, "UTC", "date-time + IANA annotation is the IANA time zone");

timeZone = "2021-08-19T17:30Z[UTC]";
const result4 = instance.toZonedDateTimeISO(timeZone);
assert.sameValue(result4.timeZoneId, "UTC", "date-time + Z + IANA annotation is the IANA time zone");

timeZone = "2021-08-19T17:30-07:00[UTC]";
const result5 = instance.toZonedDateTimeISO(timeZone);
assert.sameValue(result5.timeZoneId, "UTC", "date-time + offset + IANA annotation is the IANA time zone");
