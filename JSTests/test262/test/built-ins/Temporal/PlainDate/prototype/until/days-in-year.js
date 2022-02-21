// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: until() should take length of year into account.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainDate1 = Temporal.PlainDate.from("2019-01-01");
const plainDate2 = Temporal.PlainDate.from("2020-01-01");
const plainDate3 = Temporal.PlainDate.from("2021-01-01");
TemporalHelpers.assertDuration(plainDate1.until(plainDate2), 0, 0, 0, /* days = */ 365, 0, 0, 0, 0, 0, 0, "From January 2019");
TemporalHelpers.assertDuration(plainDate2.until(plainDate3), 0, 0, 0, /* days = */ 366, 0, 0, 0, 0, 0, 0, "From January 2020");

const plainDate4 = Temporal.PlainDate.from("2019-06-01");
const plainDate5 = Temporal.PlainDate.from("2020-06-01");
const plainDate6 = Temporal.PlainDate.from("2021-06-01");
TemporalHelpers.assertDuration(plainDate4.until(plainDate5), 0, 0, 0, /* days = */ 366, 0, 0, 0, 0, 0, 0, "From June 2019");
TemporalHelpers.assertDuration(plainDate5.until(plainDate6), 0, 0, 0, /* days = */ 365, 0, 0, 0, 0, 0, 0, "From June 2020");
