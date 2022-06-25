// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: RangeError thrown if relativeTo is a string with the wrong format
features: [Temporal]
---*/

['bad string', '15:30:45.123456', 'iso8601', 'UTC', 'P1YT1H'].forEach((relativeTo) => {
  const duration1 = new Temporal.Duration(0, 0, 0, 31);
  const duration2 = new Temporal.Duration(0, 1);
  assert.throws(RangeError, () => Temporal.Duration.compare(duration1, duration2, { relativeTo }));
});
