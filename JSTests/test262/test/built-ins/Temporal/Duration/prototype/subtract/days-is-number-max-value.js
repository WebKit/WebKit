// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: >
  BalanceDuration throws a RangeError when the result is too large.
features: [Temporal]
---*/

var duration1 = Temporal.Duration.from({days: Number.MAX_VALUE});
var duration2 = Temporal.Duration.from({days: -Number.MAX_VALUE});

assert.throws(RangeError, () => duration1.subtract(duration2));
