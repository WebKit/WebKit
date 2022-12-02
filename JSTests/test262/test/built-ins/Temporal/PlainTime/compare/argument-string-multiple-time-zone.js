// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.compare
description: More than one time zone annotation is not syntactical
features: [Temporal]
---*/

const invalidStrings = [
  "00:00[UTC][UTC]",
  "T00:00[UTC][UTC]",
  "1970-01-01T00:00[UTC][UTC]",
  "1970-01-01T00:00[!UTC][UTC]",
  "1970-01-01T00:00[UTC][!UTC]",
  "1970-01-01T00:00[UTC][u-ca=iso8601][UTC]",
  "1970-01-01T00:00[UTC][foo=bar][UTC]",
];

invalidStrings.forEach((arg) => {
  assert.throws(
    RangeError,
    () => Temporal.PlainTime.compare(arg, new Temporal.PlainTime(12, 34, 56, 987, 654, 321)),
    `reject more than one time zone annotation: ${arg} (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainTime.compare(new Temporal.PlainTime(12, 34, 56, 987, 654, 321), arg),
    `reject more than one time zone annotation: ${arg} (second argument)`
  );
});
