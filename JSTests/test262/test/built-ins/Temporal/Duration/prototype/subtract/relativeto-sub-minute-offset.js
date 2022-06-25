// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: relativeTo string accepts an inexact UTC offset rounded to hours and minutes
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 1);

let relativeTo = "2000-01-01T00:00+00:45[+00:44:30.123456789]";
const result = instance.subtract(new Temporal.Duration(0, 0, 0, 0, 24), { relativeTo });
TemporalHelpers.assertDuration(result, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "rounded HH:MM is accepted in string");

relativeTo = "2000-01-01T00:00+00:44:30[+00:44:30.123456789]";
assert.throws(RangeError, () => instance.subtract(new Temporal.Duration(0, 0, 0, 0, 24), { relativeTo }), "no other rounding is accepted for offset");

const timeZone = new Temporal.TimeZone("+00:44:30.123456789");
relativeTo = { year: 2000, month: 1, day: 1, offset: "+00:45", timeZone };
assert.throws(RangeError, () => instance.subtract(new Temporal.Duration(0, 0, 0, 0, 24), { relativeTo }), "rounded HH:MM not accepted as offset in property bag");
