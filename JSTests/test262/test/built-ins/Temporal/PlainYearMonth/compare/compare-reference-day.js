// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: compare() takes the reference day into account
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const ym1 = new Temporal.PlainYearMonth(2000, 1, iso, 1);
const ym2 = new Temporal.PlainYearMonth(2000, 1, iso, 2);
assert.sameValue(Temporal.PlainYearMonth.compare(ym1, ym2), -1);
