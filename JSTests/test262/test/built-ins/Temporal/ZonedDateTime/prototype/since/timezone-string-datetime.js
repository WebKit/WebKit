// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let expectedTimeZone = "UTC";
const instance1 = new Temporal.ZonedDateTime(0n, expectedTimeZone);

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => instance1.since({ year: 2020, month: 5, day: 2, timeZone }), "bare date-time string is not a time zone");
assert.throws(RangeError, () => instance1.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } }), "bare date-time string is not a time zone");

// The following are all valid strings so should not throw. They should produce
// expectedTimeZone, so additionally the operation will not throw due to the
// time zones being different on the receiver and the argument.

timeZone = "2021-08-19T17:30Z";
instance1.since({ year: 2020, month: 5, day: 2, timeZone });
instance1.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });

expectedTimeZone = "-07:00";
const instance2 = new Temporal.ZonedDateTime(0n, expectedTimeZone);
timeZone = "2021-08-19T17:30-07:00";
instance2.since({ year: 2020, month: 5, day: 2, timeZone });
instance2.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });

expectedTimeZone = "UTC";
const instance3 = new Temporal.ZonedDateTime(0n, expectedTimeZone);
timeZone = "2021-08-19T17:30[UTC]";
instance3.since({ year: 2020, month: 5, day: 2, timeZone });
instance3.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });

timeZone = "2021-08-19T17:30Z[UTC]";
instance3.since({ year: 2020, month: 5, day: 2, timeZone });
instance3.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });

timeZone = "2021-08-19T17:30-07:00[UTC]";
instance3.since({ year: 2020, month: 5, day: 2, timeZone });
instance3.since({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });
