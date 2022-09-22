// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Returns ±∞ when total is not representable as a Number value.
features: [Temporal]
---*/

const instance = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE, 0);

assert.sameValue(instance.total('nanoseconds'), Infinity);
