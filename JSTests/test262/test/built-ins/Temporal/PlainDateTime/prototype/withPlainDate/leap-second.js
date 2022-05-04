// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Leap second is a valid ISO string for PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);

let arg = "2016-12-31T23:59:60";
const result1 = instance.withPlainDate(arg);
TemporalHelpers.assertPlainDateTime(
  result1,
  2016, 12, "M12", 31, 12, 34, 56, 987, 654, 321,
  "leap second is a valid ISO string for PlainDate"
);

arg = { year: 2016, month: 12, day: 31, hour: 23, minute: 59, second: 60 };
const result2 = instance.withPlainDate(arg);
TemporalHelpers.assertPlainDateTime(
  result2,
  2016, 12, "M12", 31, 12, 34, 56, 987, 654, 321,
  "second: 60 is ignored in property bag for PlainDate"
);
