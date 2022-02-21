// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: add() takes month length into account
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const ym = Temporal.PlainYearMonth.from("2019-11");

TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2019-02").add({ days: 27 }),
  2019, 2, "M02");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2019-02").add({ days: 28 }),
  2019, 3, "M03");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2020-02").add({ days: 28 }),
  2020, 2, "M02");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2020-02").add({ days: 29 }),
  2020, 3, "M03");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2019-11").add({ days: 29 }),
  2019, 11, "M11");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2019-11").add({ days: 30 }),
  2019, 12, "M12");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2020-01").add({ days: 30 }),
  2020, 1, "M01");
TemporalHelpers.assertPlainYearMonth(Temporal.PlainYearMonth.from("2020-01").add({ days: 31 }),
  2020, 2, "M02");
