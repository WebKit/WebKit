// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: relativeTo string accepts an inexact UTC offset rounded to hours and minutes
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

let relativeTo = "2000-01-01T00:00+00:45[+00:44:30.123456789]";
const result = instance.total({ unit: "days", relativeTo });
assert.sameValue(result, 367, "rounded HH:MM is accepted in string");

relativeTo = "2000-01-01T00:00+00:44:30[+00:44:30.123456789]";
assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), "no other rounding is accepted for offset");

const timeZone = new Temporal.TimeZone("+00:44:30.123456789");
relativeTo = { year: 2000, month: 1, day: 1, offset: "+00:45", timeZone };
assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), "rounded HH:MM not accepted as offset in property bag");
