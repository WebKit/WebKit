// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: >
    Calendar.mergeFields method is called with null-prototype fields objects
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckMergeFieldsPrototypePollution();
const instance = new Temporal.PlainMonthDay(5, 2, calendar);
instance.toPlainDate({ year: 2019 });
assert.sameValue(calendar.mergeFieldsCallCount, 1, "mergeFields should have been called on the calendar");
