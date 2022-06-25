// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.with
description: Basic tests for with().
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1976, 11, 18);

const withYear = plainDate.with({ year: 2019 });
TemporalHelpers.assertPlainDate(withYear, 2019, 11, "M11", 18, "with(year)");

const withMonth = plainDate.with({ month: 5 });
TemporalHelpers.assertPlainDate(withMonth, 1976, 5, "M05", 18, "with(month)");

const withMonthCode = plainDate.with({ monthCode: 'M05' });
TemporalHelpers.assertPlainDate(withMonthCode, 1976, 5, "M05", 18, "with(monthCode)");

const withDay = plainDate.with({ day: 17 });
TemporalHelpers.assertPlainDate(withDay, 1976, 11, "M11", 17, "with(day)");

const withPlural = plainDate.with({ months: 12, day: 15 });
TemporalHelpers.assertPlainDate(withPlural, 1976, 11, "M11", 15, "with(plural)");

assert.throws(RangeError, () => plainDate.with({ month: 5, monthCode: 'M06' }));
