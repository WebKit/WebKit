// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: referenceISOYear argument, if given, can cause RangeError
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");

assert.throws(RangeError, () => new Temporal.PlainMonthDay(9, 14, calendar, 275760), "after the maximum ISO date");
assert.throws(RangeError, () => new Temporal.PlainMonthDay(4, 18, calendar, -271821), "before the minimum ISO date")
