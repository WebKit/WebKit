// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: An invalid ISO string is never supported
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.PlainYearMonth.from("2020-13", { overflow: "reject" }));
assert.throws(RangeError, () => Temporal.PlainYearMonth.from("2020-13", { overflow: "constrain" }));
