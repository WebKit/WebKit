// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const arg = "-000000-10-31";
const instance = new Temporal.Calendar("iso8601");

assert.throws(
    RangeError,
    () => { instance.daysInYear(arg); },
    "reject minus zero as extended year"
);
