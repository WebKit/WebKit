// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: The relativeTo option accepts a ZonedDateTime-like ISO 8601 string
features: [Temporal]
---*/

[
  '2000-01-01[UTC]',
  '2000-01-01T00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC]',
  '2000-01-01T00:00+00:00[UTC][u-ca=iso8601]',
].forEach((relativeTo) => {
  const duration = new Temporal.Duration(0, 0, 0, 31);
  const result = duration.total({ unit: "months", relativeTo });
  assert.sameValue(result, 1);
});
