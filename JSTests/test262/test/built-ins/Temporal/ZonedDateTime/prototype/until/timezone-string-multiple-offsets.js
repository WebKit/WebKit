// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const expectedTimeZone = "+01:45:30.987654321";
const instance = new Temporal.ZonedDateTime(0n, expectedTimeZone);
const timeZone = "2021-08-19T17:30:45.123456789+01:46[+01:45:30.987654321]";

// These operations should produce expectedTimeZone, so the following operations
// should not throw due to the time zones being different on the receiver and
// the argument.

instance.until({ year: 2020, month: 5, day: 2, timeZone });
instance.until({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });
