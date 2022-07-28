// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
description: >
    Calendar.mergeFields method is called with null-prototype fields objects
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckMergeFieldsPrototypePollution();
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
instance.with({ day: 24 });
assert.sameValue(calendar.mergeFieldsCallCount, 1, "mergeFields should have been called on the calendar");
