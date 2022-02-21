// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);
const timeZone = "2021-08-19T17:30:45.123456789+01:46[+01:45:30.987654321]";

const result1 = instance.toString({ timeZone });
assert.sameValue(result1.substr(-6), "+01:46", "Time zone string determined from offset");
const result2 = instance.toString({ timeZone: { timeZone } });
assert.sameValue(result2.substr(-6), "+01:46", "Time zone string determined from offset");
