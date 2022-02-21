// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-totemporaltime
description: Strings with non-ISO calendars are not supported.
info: |
    b. Let result be ? ParseTemporalTimeString(string).
    d. If result.[[Calendar]] is not one of undefined or "iso8601", then
        i. Throw a RangeError exception.
features: [Temporal]
---*/

const isoString = "2004-03-21T10:00:00";

const valid = [
  "",
  "[u-ca=iso8601]",
];

for (const s of valid) {
  const input = isoString + s;
  const plainTime = Temporal.PlainTime.from(input);
  assert.sameValue(plainTime.calendar.id, "iso8601");
}

const invalid = [
  "[u-ca=indian]",
  "[u-ca=hebrew]",
];

for (const s of invalid) {
  const input = isoString + s;
  assert.throws(RangeError, () => Temporal.PlainTime.from(input));
}
