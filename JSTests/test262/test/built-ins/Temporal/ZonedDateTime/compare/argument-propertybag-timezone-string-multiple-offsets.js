// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1588371269_012_345_679n, "+01:45:30.987654321");

const timeZone = "2021-08-19T17:30:45.123456789+01:46[+01:45:30.987654321]";

const result1 = Temporal.ZonedDateTime.compare({ year: 2020, month: 5, day: 2, timeZone }, instance);
assert.sameValue(result1, 0, "Time zone string determined from bracket name (first argument)");
const result2 = Temporal.ZonedDateTime.compare(instance, { year: 2020, month: 5, day: 2, timeZone });
assert.sameValue(result2, 0, "Time zone string determined from bracket name (first argument)");
