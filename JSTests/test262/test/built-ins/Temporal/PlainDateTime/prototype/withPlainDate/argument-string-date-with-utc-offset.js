// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: UTC offset not valid with format that does not include a time
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const instance = new Temporal.PlainDateTime(1976, 11, 18, 15, 23);

const validStrings = [
  "2000-05-02T00+00:00",
  "2000-05-02T00+00:00[UTC]",
  "2000-05-02T00+00:00[!UTC]",
  "2000-05-02T00-02:30[America/St_Johns]",
];

for (const arg of validStrings) {
  const result = instance.withPlainDate(arg);

  TemporalHelpers.assertPlainDateTime(
    result,
    2000, 5, "M05", 2, 15, 23, 0, 0, 0, 0,
    `"${arg}" is a valid UTC offset with time for PlainDate`
  );
}

const invalidStrings = [
  "2022-09-15Z",
  "2022-09-15Z[UTC]",
  "2022-09-15Z[Europe/Vienna]",
  "2022-09-15+00:00",
  "2022-09-15+00:00[UTC]",
  "2022-09-15-02:30",
  "2022-09-15-02:30[America/St_Johns]",
];

for (const arg of invalidStrings) {
  assert.throws(
    RangeError,
    () => instance.withPlainDate(arg),
    `"${arg}" UTC offset without time is not valid for PlainDate`
  );
}
