// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.daysinyear
description: Basic tests for daysInYear.
features: [Temporal]
---*/

assert.sameValue((new Temporal.PlainDate(1976, 11, 18)).daysInYear, 366, "leap year");
assert.sameValue((new Temporal.PlainDate(1977, 11, 18)).daysInYear, 365, "non-leap year");
