// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const expectedTimeZone = "+01:45:30.987654321";
const instance = new Temporal.ZonedDateTime(0n, expectedTimeZone);
const timeZone = "2021-08-19T17:30:45.123456789+01:46[+01:45:30.987654321]";

// These operations should produce expectedTimeZone, so the following should
// be equal due to the time zones being different on the receiver and
// the argument.

const properties = { year: 1970, month: 1, day: 1, hour: 1, minute: 45, second: 30, millisecond: 987, microsecond: 654, nanosecond: 321 };
assert(instance.equals({ ...properties, timeZone }), "time zone string should produce expected time zone");
assert(instance.equals({ ...properties, timeZone: { timeZone } }), "time zone string should produce expected time zone");
