// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Throws if a ZonedDateTime-like relativeTo string has the wrong UTC offset
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);
const relativeTo = "2000-01-01T00:00+05:30[UTC]";
assert.throws(
  RangeError,
  () => instance.total({ unit: "days", relativeTo }),
  "total should throw RangeError on a string with UTC offset mismatch"
);
