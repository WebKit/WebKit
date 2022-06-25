// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Leap second is a valid ISO string for PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

let arg = "2016-12-31T23:59:60";
const result1 = instance.dateAdd(arg, new Temporal.Duration());
TemporalHelpers.assertPlainDate(
  result1,
  2016, 12, "M12", 31,
  "leap second is a valid ISO string for PlainDate"
);

arg = { year: 2016, month: 12, day: 31, hour: 23, minute: 59, second: 60 };
const result2 = instance.dateAdd(arg, new Temporal.Duration());
TemporalHelpers.assertPlainDate(
  result2,
  2016, 12, "M12", 31,
  "second: 60 is ignored in property bag for PlainDate"
);
