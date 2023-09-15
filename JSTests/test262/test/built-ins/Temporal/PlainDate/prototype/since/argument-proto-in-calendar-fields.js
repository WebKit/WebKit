// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plaindate.prototype.since
description: If a calendar's fields() method returns a field named '__proto__', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['__proto__']);
const arg = {year: 2023, month: 5, monthCode: 'M05', day: 1, calendar: calendar};
const instance = new Temporal.PlainDate(2000, 5, 2);

assert.throws(RangeError, () => instance.since(arg));
