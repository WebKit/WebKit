// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Islamic calendar "islamic-rgsa".
features: [Temporal]
---*/

assert.throws(RangeError, function () {
  Temporal.PlainDate.from({ year: 1500, month: 1, day: 1, calendar: "islamic-rgsa" });
}, "fallback for calendar ID 'islamic-rgsa' only supported in Intl.DateTimeFormat constructor, not Temporal");
