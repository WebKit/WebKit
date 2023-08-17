// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.compare
description: If a calendar's fields() method returns a field named 'constructor', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['constructor']);
const relativeTo = { year: 2023, month: 5, monthCode: 'M05', day: 1, calendar: calendar, timeZone: 'Europe/Paris' };
const duration1 = new Temporal.Duration(1);
const duration2 = new Temporal.Duration(0, 12);

assert.throws(RangeError, () => Temporal.Duration.compare(duration1, duration2, {relativeTo: relativeTo}));
