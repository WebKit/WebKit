// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaintime
description: UTC offset not valid with format that does not include a time
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const validStrings = [
  "12:34:56.987654321+00:00",
  "12:34:56.987654321+00:00[UTC]",
  "12:34:56.987654321+00:00[!UTC]",
  "12:34:56.987654321-02:30[America/St_Johns]",
  "1976-11-18T12:34:56.987654321+00:00",
  "1976-11-18T12:34:56.987654321+00:00[UTC]",
  "1976-11-18T12:34:56.987654321+00:00[!UTC]",
  "1976-11-18T12:34:56.987654321-02:30[America/St_Johns]",
];

for (const arg of validStrings) {
  const result = instance.withPlainTime(arg);

  assert.sameValue(
    result.epochNanoseconds,
    45_296_987_654_321n,
    `"${arg}" is a valid UTC offset with time for PlainTime`
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
    () => instance.withPlainTime(arg),
    `"${arg}" UTC offset without time is not valid for PlainTime`
  );
}
