// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: UTC offset not valid with format that does not include a time
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const validStrings = [
  "1976-11-18T15:23+00:00",
  "1976-11-18T15:23+00:00[UTC]",
  "1976-11-18T15:23+00:00[!UTC]",
  "1976-11-18T15:23-02:30[America/St_Johns]",
];

for (const arg of validStrings) {
  const result = instance.getInstantFor(arg);

  assert.sameValue(
    result.epochNanoseconds,
    217_178_580_000_000_000n,
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
    () => instance.getInstantFor(arg),
    `"${arg}" UTC offset without time is not valid for PlainDateTime`
  );
}
