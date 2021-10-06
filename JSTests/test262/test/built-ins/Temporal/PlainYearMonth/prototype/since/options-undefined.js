// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Verify that undefined options are handled correctly.
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2002, 12);

const explicit = later.since(earlier, undefined);
assert.sameValue(explicit.years, 2, "default largest unit is years");
assert.sameValue(explicit.months, 7, "default smallest unit is months and rounding is none");

const implicit = later.since(earlier);
assert.sameValue(implicit.years, 2, "default largest unit is years");
assert.sameValue(implicit.months, 7, "default smallest unit is months and rounding is none");
