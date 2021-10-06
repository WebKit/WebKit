// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: TimeZone.from() with bad offsets in ISO strings.
features: [Temporal]
---*/

const invalids = [
  "1994-11-05T08:15:30-05:00[UTC]",
  "1994-11-05T13:15:30+00:00[America/New_York]",
  "1994-11-05T13:15:30-03[Europe/Brussels]",
];

for (const item of invalids) {
  assert.throws(RangeError, () => Temporal.TimeZone.from(item), `Throws for ${item}`);
}
