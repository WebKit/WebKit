// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplaindatetime
description: toPlainDateTime() doesn't call into the calendar.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const plainDate = new Temporal.PlainDate(2000, 5, 2, calendar);
const result = plainDate.toPlainDateTime("11:30:23");
assert.sameValue(result.calendar, calendar, "calendar");
