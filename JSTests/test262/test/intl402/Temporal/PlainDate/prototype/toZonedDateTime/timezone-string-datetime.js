// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Conversion of ISO date-time strings to Temporal.TimeZone instances (with Intl time zones)
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);

let timeZone = "2021-08-19T17:30[America/Vancouver]";
const result1 = instance.toZonedDateTime(timeZone);
assert.sameValue(result1.timeZone.id, "America/Vancouver", "date-time + IANA annotation is the IANA time zone");
const result2 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result2.timeZone.id, "America/Vancouver", "date-time + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30Z[America/Vancouver]";
const result3 = instance.toZonedDateTime(timeZone);
assert.sameValue(result3.timeZone.id, "America/Vancouver", "date-time + Z + IANA annotation is the IANA time zone");
const result4 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result4.timeZone.id, "America/Vancouver", "date-time + Z + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00[America/Vancouver]";
const result5 = instance.toZonedDateTime(timeZone);
assert.sameValue(result5.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone");
const result6 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result6.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");
