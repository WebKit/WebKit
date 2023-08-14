// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: If a calendar's fields() method returns a field named 'constructor', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['constructor']);
const relativeTo = { year: 2023, month: 5, monthCode: 'M05', day: 1, calendar: calendar, timeZone: 'Europe/Paris' };
const instance = new Temporal.Duration(1, 0, 0, 0, 24);

assert.throws(RangeError, () => instance.round({ largestUnit: "years", relativeTo }));
