// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Handling for overflow option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const validValues = [
  new Temporal.PlainMonthDay(5, 2),
  "05-02",
];
validValues.forEach((value) => {
  const constrain = Temporal.PlainMonthDay.from(value, { overflow: "constrain" });
  TemporalHelpers.assertPlainMonthDay(constrain, "M05", 2, "overflow is ignored: constrain");

  const reject = Temporal.PlainMonthDay.from(value, { overflow: "reject" });
  TemporalHelpers.assertPlainMonthDay(reject, "M05", 2, "overflow is ignored: reject");
});

const propertyBag1 = { year: 2000, month: 13, day: 34 };
const result1 = Temporal.PlainMonthDay.from(propertyBag1, { overflow: "constrain" });
TemporalHelpers.assertPlainMonthDay(result1, "M12", 31, "default overflow is constrain");
assert.throws(RangeError, () => Temporal.PlainMonthDay.from(propertyBag1, { overflow: "reject" }),
  "invalid property bag: reject");

const propertyBag2 = { month: 1, day: 32 };
const result2 = Temporal.PlainMonthDay.from(propertyBag2, { overflow: "constrain" });
TemporalHelpers.assertPlainMonthDay(result2, "M01", 31, "default overflow is constrain");
assert.throws(RangeError, () => Temporal.PlainMonthDay.from(propertyBag2, { overflow: "reject" }),
  "invalid property bag: reject");

assert.throws(RangeError, () => Temporal.PlainMonthDay.from("13-34", { overflow: "constrain" }),
  "invalid ISO string: constrain");
assert.throws(RangeError, () => Temporal.PlainMonthDay.from("13-34", { overflow: "reject" }),
  "invalid ISO string: reject");
