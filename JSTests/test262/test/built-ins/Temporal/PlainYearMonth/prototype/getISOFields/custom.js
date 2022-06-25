// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getisofields
description: getISOFields does not call into user code.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const instance = new Temporal.PlainYearMonth(2000, 5, calendar);
const result = instance.getISOFields();

assert.sameValue(result.isoYear, 2000, "isoYear result");
assert.sameValue(result.isoMonth, 5, "isoMonth result");
assert.sameValue(result.isoDay, 1, "isoDay result");
assert.sameValue(result.calendar, calendar, "calendar result");
