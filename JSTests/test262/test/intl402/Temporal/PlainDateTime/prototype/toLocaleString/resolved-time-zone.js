// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tolocalestring
description: A time zone in resolvedOptions with a large offset still produces the correct string
locale: [en]
features: [Temporal]
---*/

const datetime1 = new Temporal.PlainDateTime(2021, 8, 4, 0, 30, 45, 123, 456, 789);
const result1 = datetime1.toLocaleString("en", { timeZone: "Pacific/Apia" });
assert.sameValue(result1, "8/4/2021, 12:30:45 AM");

const datetime2 = new Temporal.PlainDateTime(2021, 8, 4, 23, 30, 45, 123, 456, 789);
const result2 = datetime2.toLocaleString("en", { timeZone: "Pacific/Apia" });
assert.sameValue(result2, "8/4/2021, 11:30:45 PM");
