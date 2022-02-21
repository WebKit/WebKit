// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.daysinweek
description: Basic tests for daysInWeek().
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1976, 11, 18);
assert.sameValue(plainDate.daysInWeek, 7);
