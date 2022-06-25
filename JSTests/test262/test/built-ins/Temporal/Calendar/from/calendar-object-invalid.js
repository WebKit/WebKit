// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: Converting objects to Temporal.Calendar
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.Calendar.from({ calendar: "local" }));
assert.throws(RangeError, () => Temporal.Calendar.from({ calendar: { calendar: "iso8601" } }));
