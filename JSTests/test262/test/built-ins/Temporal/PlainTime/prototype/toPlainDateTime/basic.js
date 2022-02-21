// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.toplaindatetime
description: Basic tests for toPlainDateTime().
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const plainTime = Temporal.PlainTime.from("11:30:23.123456789");

const plainDate = plainTime.toPlainDateTime(Temporal.PlainDate.from("1976-11-18"));
TemporalHelpers.assertPlainDateTime(plainDate, 1976, 11, "M11", 18, 11, 30, 23, 123, 456, 789, "PlainDate");

const optionBag = plainTime.toPlainDateTime({ year: 1976, month: 11, day: 18 });
TemporalHelpers.assertPlainDateTime(optionBag, 1976, 11, "M11", 18, 11, 30, 23, 123, 456, 789, "option bag");

const string = plainTime.toPlainDateTime("1976-11-18");
TemporalHelpers.assertPlainDateTime(string, 1976, 11, "M11", 18, 11, 30, 23, 123, 456, 789, "string");

assert.throws(TypeError, () => plainTime.toPlainDateTime({ year: 1976 }), "missing properties");
