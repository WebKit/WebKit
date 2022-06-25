// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => Temporal.TimeZone.from(timeZone), "bare date-time string is not a time zone");
assert.throws(RangeError, () => Temporal.TimeZone.from({ timeZone }), "bare date-time string is not a time zone");

timeZone = "2021-08-19T17:30Z";
const result1 = Temporal.TimeZone.from(timeZone);
assert.sameValue(result1.id, "UTC", "date-time + Z is UTC time zone");
const result2 = Temporal.TimeZone.from({ timeZone });
assert.sameValue(result2.id, "UTC", "date-time + Z is UTC time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00";
const result3 = Temporal.TimeZone.from(timeZone);
assert.sameValue(result3.id, "-07:00", "date-time + offset is the offset time zone");
const result4 = Temporal.TimeZone.from({ timeZone });
assert.sameValue(result4.id, "-07:00", "date-time + offset is the offset time zone (string in property bag)");

timeZone = "2021-08-19T17:30[UTC]";
const result5 = Temporal.TimeZone.from(timeZone);
assert.sameValue(result5.id, "UTC", "date-time + IANA annotation is the IANA time zone");
const result6 = Temporal.TimeZone.from({ timeZone });
assert.sameValue(result6.id, "UTC", "date-time + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30Z[UTC]";
const result7 = Temporal.TimeZone.from(timeZone);
assert.sameValue(result7.id, "UTC", "date-time + Z + IANA annotation is the IANA time zone");
const result8 = Temporal.TimeZone.from({ timeZone });
assert.sameValue(result8.id, "UTC", "date-time + Z + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00[UTC]";
const result9 = Temporal.TimeZone.from(timeZone);
assert.sameValue(result9.id, "UTC", "date-time + offset + IANA annotation is the IANA time zone");
const result10 = Temporal.TimeZone.from({ timeZone });
assert.sameValue(result10.id, "UTC", "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");
