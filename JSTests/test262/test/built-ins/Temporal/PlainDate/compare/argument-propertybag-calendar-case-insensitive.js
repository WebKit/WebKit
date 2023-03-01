// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const calendar = "IsO8601";

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18));
assert.sameValue(result1, 0, "Calendar is case-insensitive (first argument)");
const result2 = Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg);
assert.sameValue(result2, 0, "Calendar is case-insensitive (second argument)");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result3 = Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18));
assert.sameValue(result3, 0, "Calendar is case-insensitive (nested property, first argument)");
const result4 = Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg);
assert.sameValue(result4, 0, "Calendar is case-insensitive (nested property, second argument)");
