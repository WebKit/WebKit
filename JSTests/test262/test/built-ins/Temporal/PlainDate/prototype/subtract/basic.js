// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: Basic tests
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const date = Temporal.PlainDate.from("2019-11-18");
TemporalHelpers.assertPlainDate(date.subtract({ years: 43 }),
  1976, 11, "M11", 18);
TemporalHelpers.assertPlainDate(date.subtract({ months: 11 }),
  2018, 12, "M12", 18);
TemporalHelpers.assertPlainDate(date.subtract({ days: 20 }),
  2019, 10, "M10", 29);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2019-02-28').subtract({ months: 1 }),
  2019, 1, "M01", 28);
TemporalHelpers.assertPlainDate(date.subtract(Temporal.Duration.from('P43Y')),
  1976, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('1976-11-18').subtract({ years: -43 }),
  2019, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2018-12-18').subtract({ months: -11 }),
  2019, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2019-10-29').subtract({ days: -20 }),
  2019, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2019-01-28').subtract({ months: -1 }),
  2019, 2, "M02", 28);
