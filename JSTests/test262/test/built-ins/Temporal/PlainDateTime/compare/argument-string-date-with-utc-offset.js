// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: UTC offset not valid with format that does not include a time
features: [Temporal]
---*/

const validStrings = [
  "1976-11-18T15:23+00:00",
  "1976-11-18T15:23+00:00[UTC]",
  "1976-11-18T15:23+00:00[!UTC]",
  "1976-11-18T15:23-02:30[America/St_Johns]",
];

for (const arg of validStrings) {
  const result = Temporal.PlainDateTime.compare(arg, arg);

  assert.sameValue(
    result,
    0,
    `"${arg}" is a valid UTC offset with time for PlainDateTime`
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
    () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)),
    `"${arg}" UTC offset without time is not valid for PlainDateTime (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg),
    `"${arg}" UTC offset without time is not valid for PlainDateTime (second argument)`
  );
}
