// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Property bag with offset property is rejected if offset is in the wrong format
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const badOffsets = [
  "00:00",    // missing sign
  "+0",       // too short
  "-000:00",  // too long
  0,          // converts to a string that is invalid
];
badOffsets.forEach((offset) => {
  const arg = { year: 2021, month: 10, day: 28, offset, timeZone };
  assert.throws(RangeError, () => instance.equals(arg), `"${offset} is not a valid offset string`);
});
