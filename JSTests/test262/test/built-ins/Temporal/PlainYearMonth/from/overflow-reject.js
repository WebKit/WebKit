// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Reject value for overflow option
features: [Temporal]
---*/

const bad = { year: 2019, month: 13 };
assert.throws(RangeError, () => Temporal.PlainYearMonth.from(bad, { overflow: "reject" }));
