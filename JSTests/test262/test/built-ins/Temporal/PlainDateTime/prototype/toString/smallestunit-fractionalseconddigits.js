// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: fractionalSecondDigits option is not used with smallestUnit present
features: [Temporal]
---*/

const datetime = new Temporal.PlainDateTime(1976, 11, 18, 12, 34, 56, 789, 999, 999);
const tests = [
  ["minute", "1976-11-18T12:34"],
  ["second", "1976-11-18T12:34:56"],
  ["millisecond", "1976-11-18T12:34:56.789"],
  ["microsecond", "1976-11-18T12:34:56.789999"],
  ["nanosecond", "1976-11-18T12:34:56.789999999"],
];

for (const [smallestUnit, expected] of tests) {
  const string = datetime.toString({
    smallestUnit,
    get fractionalSecondDigits() { throw new Test262Error("should not get fractionalSecondDigits") }
  });
  assert.sameValue(string, expected, `smallestUnit: "${smallestUnit}" overrides fractionalSecondDigits`);
}

assert.throws(RangeError, () => datetime.toString({
  smallestUnit: "hour",
  get fractionalSecondDigits() { throw new Test262Error("should not get fractionalSecondDigits") }
}), "hour is an invalid smallestUnit but still overrides fractionalSecondDigits");
