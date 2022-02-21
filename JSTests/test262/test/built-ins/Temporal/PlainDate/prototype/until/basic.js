// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Basic tests for until().
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1969, 7, 24);
const plainDate2 = Temporal.PlainDate.from({ year: 1969, month: 10, day: 5 });
TemporalHelpers.assertDuration(plainDate.until(plainDate2), 0, 0, 0, /* days = */ 73, 0, 0, 0, 0, 0, 0, "same year");

const earlier = new Temporal.PlainDate(1969, 7, 24);
const later = new Temporal.PlainDate(1996, 3, 3);
TemporalHelpers.assertDuration(earlier.until(later), 0, 0, 0, /* days = */ 9719, 0, 0, 0, 0, 0, 0, "different year");

TemporalHelpers.assertDuration(plainDate.until({ year: 2019, month: 7, day: 24 }), 0, 0, 0, /* days = */ 18262, 0, 0, 0, 0, 0, 0, "option bag");
TemporalHelpers.assertDuration(plainDate.until("2019-07-24"), 0, 0, 0, /* days = */ 18262, 0, 0, 0, 0, 0, 0, "string");
