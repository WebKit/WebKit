// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetimeiso
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => Temporal.Now.zonedDateTimeISO(timeZone), "bare date-time string is not a time zone");
assert.throws(RangeError, () => Temporal.Now.zonedDateTimeISO({ timeZone }), "bare date-time string is not a time zone");

timeZone = "2021-08-19T17:30Z";
const result1 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result1.timeZone.id, "UTC", "date-time + Z is UTC time zone");
const result2 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result2.timeZone.id, "UTC", "date-time + Z is UTC time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00";
const result3 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result3.timeZone.id, "-07:00", "date-time + offset is the offset time zone");
const result4 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result4.timeZone.id, "-07:00", "date-time + offset is the offset time zone (string in property bag)");

timeZone = "2021-08-19T17:30-0700";
const result5 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result5.timeZone.id, "-07:00", "date-time + offset is the offset time zone");
const result6 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result6.timeZone.id, "-07:00", "date-time + offset is the offset time zone (string in property bag)");

timeZone = "2021-08-19T17:30[America/Vancouver]";
const result7 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result7.timeZone.id, "America/Vancouver", "date-time + IANA annotation is the IANA time zone");
const result8 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result8.timeZone.id, "America/Vancouver", "date-time + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30Z[America/Vancouver]";
const result9 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result9.timeZone.id, "America/Vancouver", "date-time + Z + IANA annotation is the IANA time zone");
const result10 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result10.timeZone.id, "America/Vancouver", "date-time + Z + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30-07:00[America/Vancouver]";
const result11 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result11.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone");
const result12 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result12.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-08-19T17:30-0700[America/Vancouver]";
const result13 = Temporal.Now.zonedDateTimeISO(timeZone);
assert.sameValue(result13.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone");
const result14 = Temporal.Now.zonedDateTimeISO({ timeZone });
assert.sameValue(result14.timeZone.id, "America/Vancouver", "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");
