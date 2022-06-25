// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tolocalestring
description: A time zone in resolvedOptions with a large offset still produces the correct string
locale: [en]
features: [Temporal]
---*/

const time1 = new Temporal.PlainTime(0, 30, 45, 123, 456, 789);
const result1 = time1.toLocaleString("en", { timeZone: "Pacific/Apia" });
assert.sameValue(result1, "12:30:45 AM");

const time2 = new Temporal.PlainTime(23, 30, 45, 123, 456, 789);
const result2 = time2.toLocaleString("en", { timeZone: "Pacific/Apia" });
assert.sameValue(result2, "11:30:45 PM");
