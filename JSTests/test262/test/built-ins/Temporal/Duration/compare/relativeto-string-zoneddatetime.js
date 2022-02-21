// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: The relativeTo option accepts a ZonedDateTime-like ISO 8601 string
features: [Temporal]
---*/

[
  '2000-01-01[UTC]',
  '2000-01-01T00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC][u-ca=iso8601]',
].forEach((relativeTo) => {
  const duration1 = new Temporal.Duration(0, 0, 0, 31);
  const duration2 = new Temporal.Duration(0, 1);
  const result = Temporal.Duration.compare(duration1, duration2, { relativeTo });
  assert.sameValue(result, 0);
});
