// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.since
description: RangeError thrown when smallestUnit option not one of the allowed string values
features: [Temporal]
---*/

const earlier = new Temporal.Instant(1_000_000_000_000_000_000n);
const later = new Temporal.Instant(1_000_090_061_987_654_321n);
const values = ["era", "eraYear", "years", "months", "weeks", "days", "other string"];
for (const smallestUnit of values) {
  assert.throws(RangeError, () => later.since(earlier, { smallestUnit }));
}
