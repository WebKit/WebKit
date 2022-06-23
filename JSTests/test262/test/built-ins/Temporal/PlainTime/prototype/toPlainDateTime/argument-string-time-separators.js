// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: Time separator in string argument can vary
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const tests = [
  ["2000-05-02T15:23", "uppercase T"],
  ["2000-05-02t15:23", "lowercase T"],
  ["2000-05-02 15:23", "space between date and time"],
];

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

tests.forEach(([arg, description]) => {
  const result = instance.toPlainDateTime(arg);

  TemporalHelpers.assertPlainDateTime(
    result,
    2000, 5, "M05", 2, 12, 34, 56, 987, 654, 321,
    `variant time separators (${description})`
  );
});
