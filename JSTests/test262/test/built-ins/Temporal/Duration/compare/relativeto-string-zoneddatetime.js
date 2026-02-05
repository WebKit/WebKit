// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: The relativeTo option accepts a ZonedDateTime-like ISO 8601 string
features: [Temporal]
---*/

const duration1 = new Temporal.Duration(0, 0, 0, 31);
const duration2 = new Temporal.Duration(0, 1);

[
  '2000-01-01[UTC]',
  '2000-01-01T00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC][u-ca=iso8601]',
].forEach((relativeTo) => {
  const result = Temporal.Duration.compare(duration1, duration2, { relativeTo });
  assert.sameValue(result, 0);
});

[
  '2025-01-01T00:00:00+00:0000[UTC]'
].forEach((relativeTo) => {
  assert.throws(RangeError, () => Temporal.Duration.compare(duration1, duration2, { relativeTo }), "separators in offset are inconsistent");
});
