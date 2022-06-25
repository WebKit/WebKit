// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.year
description: Leap second is a valid ISO string for PlainDate
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

let arg = "2016-12-31T23:59:60";
const result1 = instance.year(arg);
assert.sameValue(
  result1,
  2016,
  "leap second is a valid ISO string for PlainDate"
);

arg = { year: 2016, month: 12, day: 31, hour: 23, minute: 59, second: 60 };
const result2 = instance.year(arg);
assert.sameValue(
  result2,
  2016,
  "second: 60 is ignored in property bag for PlainDate"
);
