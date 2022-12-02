// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Various forms of unknown annotation
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const tests = [
  ["2000-05-02[foo=bar]", "without time"],
  ["2000-05-02T15:23[foo=bar]", "alone"],
  ["2000-05-02T15:23[UTC][foo=bar]", "with time zone"],
  ["2000-05-02T15:23[u-ca=iso8601][foo=bar]", "with calendar"],
  ["2000-05-02T15:23[UTC][foo=bar][u-ca=iso8601]", "with time zone and calendar"],
  ["2000-05-02T15:23[foo=bar][_foo-bar0=Ignore-This-999999999999]", "with another unknown annotation"],
];

const instance = new Temporal.PlainDateTime(1976, 11, 18, 15, 23);

tests.forEach(([arg, description]) => {
  const result = instance.withPlainDate(arg);

  TemporalHelpers.assertPlainDateTime(
    result,
    2000, 5, "M05", 2, 15, 23, 0, 0, 0, 0,
    `unknown annotation (${description})`
  );
});
