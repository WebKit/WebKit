// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: relativeTo property bag with offset property is rejected if offset is in the wrong format
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const d1 = new Temporal.Duration(0, 1, 0, 280);
const d2 = new Temporal.Duration(0, 1, 0, 281);

const badOffsets = [
  "00:00",    // missing sign
  "+0",       // too short
  "-000:00",  // too long
  0,          // converts to a string that is invalid
];
badOffsets.forEach((offset) => {
  const relativeTo = { year: 2021, month: 10, day: 28, offset, timeZone };
  assert.throws(RangeError, () => Temporal.Duration.compare(d1, d2, { relativeTo }), `"${offset} is not a valid offset string`);
});
