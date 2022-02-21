// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Basic tests
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const date = Temporal.PlainDate.from("1976-11-18");
TemporalHelpers.assertPlainDate(date.add({ years: 43 }),
  2019, 11, "M11", 18);
TemporalHelpers.assertPlainDate(date.add({ months: 3 }),
  1977, 2, "M02", 18);
TemporalHelpers.assertPlainDate(date.add({ days: 20 }),
  1976, 12, "M12", 8);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from("2019-01-31").add({ months: 1 }),
  2019, 2, "M02", 28);
TemporalHelpers.assertPlainDate(date.add(Temporal.Duration.from('P43Y')),
  2019, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2019-11-18').add({ years: -43 }),
  1976, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('1977-02-18').add({ months: -3 }),
  1976, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('1976-12-08').add({ days: -20 }),
  1976, 11, "M11", 18);
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from('2019-02-28').add({ months: -1 }),
  2019, 1, "M01", 28);
