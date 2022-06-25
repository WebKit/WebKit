// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => instance.toZonedDateTime(timeZone), "bare date-time string is not a time zone");
assert.throws(RangeError, () => instance.toZonedDateTime({ timeZone }), "bare date-time string is not a time zone");

timeZone = "2021-08-19T17:30Z";
const result1 = instance.toZonedDateTime(timeZone);
assert.sameValue(result1.timeZone.id, "UTC", "date-time + Z is UTC time zone");
const result2 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result2.timeZone.id, "UTC", "date-time + Z is UTC time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00";
const result3 = instance.toZonedDateTime(timeZone);
assert.sameValue(result3.timeZone.id, "-07:00", "date-time + offset is the offset time zone");
const result4 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result4.timeZone.id, "-07:00", "date-time + offset is the offset time zone (string in property bag)");

timeZone = "2021-08-19T17:30[UTC]";
const result5 = instance.toZonedDateTime(timeZone);
assert.sameValue(result5.timeZone.id, "UTC", "date-time + IANA annotation is the IANA time zone");
const result6 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result6.timeZone.id, "UTC", "date-time + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30Z[UTC]";
const result7 = instance.toZonedDateTime(timeZone);
assert.sameValue(result7.timeZone.id, "UTC", "date-time + Z + IANA annotation is the IANA time zone");
const result8 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result8.timeZone.id, "UTC", "date-time + Z + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00[UTC]";
const result9 = instance.toZonedDateTime(timeZone);
assert.sameValue(result9.timeZone.id, "UTC", "date-time + offset + IANA annotation is the IANA time zone");
const result10 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result10.timeZone.id, "UTC", "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");
