// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.add
description: RangeError thrown if result is outside representable range
features: [Temporal]
---*/

const fields = ["hours", "minutes", "seconds", "milliseconds", "microseconds", "nanoseconds"];

const instance = Temporal.Instant.fromEpochNanoseconds(8640000_000_000_000_000_000n);

fields.forEach((field) => {
  assert.throws(RangeError, () => instance.add({ [field]: 1 }));
});
