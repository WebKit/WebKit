// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Property bag is correctly converted into PlainDate
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const dateTimeFields = { year: 2019, month: 10, monthCode: "M10", day: 1, hour: 14, minute: 20, second: 36 };
const plainDate = Temporal.PlainDate.from(dateTimeFields);
TemporalHelpers.assertPlainDate(plainDate, 2019, 10, "M10", 1);

const badFields = { year: 2019, month: 1, day: 32 };
assert.throws(RangeError, () => Temporal.PlainDate.from(badFields, { overflow: "reject" }),
  "bad fields with reject");
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from(badFields),
  2019, 1, "M01", 31, "bad fields with missing overflow");
TemporalHelpers.assertPlainDate(Temporal.PlainDate.from(badFields, { overflow: "constrain" }),
  2019, 1, "M01", 31, "bad fields with constrain");
