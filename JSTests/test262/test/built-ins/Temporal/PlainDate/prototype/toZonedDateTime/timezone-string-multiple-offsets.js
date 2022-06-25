// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);
const timeZone = "2021-08-19T17:30:45.123456789+01:46[+01:45:30.987654321]";

const result1 = instance.toZonedDateTime(timeZone);
assert.sameValue(result1.timeZone.id, "+01:45:30.987654321", "Time zone string determined from bracket name");
const result2 = instance.toZonedDateTime({ timeZone });
assert.sameValue(result2.timeZone.id, "+01:45:30.987654321", "Time zone string determined from bracket name");
