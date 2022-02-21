// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let expectedTimeZone = "UTC";
const instance1 = new Temporal.ZonedDateTime(0n, expectedTimeZone);

let timeZone = "2021-02-19T17:30";
assert.throws(RangeError, () => instance1.equals({ year: 1970, month: 1, day: 1, timeZone }), "bare date-time string is not a time zone");
assert.throws(RangeError, () => instance1.equals({ year: 1970, month: 1, day: 1, timeZone: { timeZone } }), "bare date-time string is not a time zone");

// The following are all valid strings so should not throw. They should produce
// expectedTimeZone, so additionally the operation should return true, because
// the property bag will produce an instance that's equal to the receiver.

timeZone = "2021-02-19T17:30Z";
assert(instance1.equals({ year: 1970, month: 1, day: 1, timeZone }), "date-time + Z is UTC time zone");
assert(instance1.equals({ year: 1970, month: 1, day: 1, timeZone: { timeZone } }), "date-time + Z is UTC time zone (string in property bag)");

expectedTimeZone = "-08:00";
const instance2 = new Temporal.ZonedDateTime(0n, expectedTimeZone);
timeZone = "2021-02-19T17:30-08:00";
assert(instance2.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone }), "date-time + offset is the offset time zone");
assert(instance2.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone: { timeZone } }), "date-time + offset is the offset time zone (string in property bag)");

const instance3 = new Temporal.ZonedDateTime(0n, expectedTimeZone);
timeZone = "2021-02-19T17:30[-08:00]";
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone }), "date-time + IANA annotation is the IANA time zone");
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone: { timeZone } }), "date-time + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-02-19T17:30Z[-08:00]";
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone }), "date-time + Z + IANA annotation is the IANA time zone");
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone: { timeZone } }), "date-time + Z + IANA annotation is the IANA time zone (string in property bag)");

timeZone = "2021-02-19T17:30-08:00[-08:00]";
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone }), "date-time + offset + IANA annotation is the IANA time zone");
assert(instance3.equals({ year: 1969, month: 12, day: 31, hour: 16, timeZone: { timeZone } }), "date-time + offset + IANA annotation is the IANA time zone (string in property bag)");
