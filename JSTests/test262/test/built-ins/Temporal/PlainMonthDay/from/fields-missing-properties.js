// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Basic tests for PlainMonthDay.from(object) with missing properties.
features: [Temporal]
---*/

assert.throws(TypeError, () => Temporal.PlainMonthDay.from({}), "No properties");
assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ day: 15 }), "Only day");
assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ monthCode: 'M12' }), "Only monthCode");
assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ monthCode: undefined, day: 15 }), "monthCode undefined");
assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ months: 12, day: 31 }), "months plural");
assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ month: 11, day: 18, calendar: "iso8601" }), "month, day with calendar");
