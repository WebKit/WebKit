// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: Calendar is not taken into account for the comparison.
features: [Temporal]
---*/

const calendar1 = { toString() { throw new Test262Error("should not call calendar1.toString") } };
const calendar2 = { toString() { throw new Test262Error("should not call calendar2.toString") } };
const dt1 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar1);
const dt2 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102, calendar1);
const dt3 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102, calendar1);
const dt4 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102, calendar2);

assert.sameValue(Temporal.PlainDateTime.compare(dt1, dt2), -1, "smaller");
assert.sameValue(Temporal.PlainDateTime.compare(dt2, dt3), 0, "equal with same calendar");
assert.sameValue(Temporal.PlainDateTime.compare(dt2, dt4), 0, "equal with different calendar");
