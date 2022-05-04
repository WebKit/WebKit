// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Leap second is a valid ISO string for PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

let arg = "2016-12-31T23:59:60";
let result = instance.dateUntil(arg, new Temporal.PlainDate(2017, 1, 1));
TemporalHelpers.assertDuration(result, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "leap second is a valid ISO string for PlainDate (first argument)");
result = instance.dateUntil(new Temporal.PlainDate(2017, 1, 1), arg);
TemporalHelpers.assertDuration(result, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "leap second is a valid ISO string for PlainDate (second argument)");

arg = { year: 2016, month: 12, day: 31, hour: 23, minute: 59, second: 60 };
result = instance.dateUntil(arg, new Temporal.PlainDate(2017, 1, 1));
TemporalHelpers.assertDuration(result, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "second: 60 is ignored in property bag for PlainDate (first argument)");
result = instance.dateUntil(new Temporal.PlainDate(2017, 1, 1), arg);
TemporalHelpers.assertDuration(result, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "second: 60 is ignored in property bag for PlainDate (second argument)");
