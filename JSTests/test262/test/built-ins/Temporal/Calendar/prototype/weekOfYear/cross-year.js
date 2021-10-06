// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: weekOfYear() crossing year boundaries.
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
assert.sameValue(iso.weekOfYear(Temporal.PlainDate.from("2019-12-31")), 1, "week 1 from next year");
assert.sameValue(iso.weekOfYear(Temporal.PlainDate.from("2021-01-01")), 53, "week 1 from next year");
