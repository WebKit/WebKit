// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration
description: Undefined arguments should be treated as zero.
features: [Temporal]
---*/

const args = [1, 1, 1, 1, 1, 1, 1, 1];

const explicit = new Temporal.Duration(...args, undefined);
assert.sameValue(explicit.microseconds, 0, "microseconds default argument");

const implicit = new Temporal.Duration(...args);
assert.sameValue(implicit.microseconds, 0, "microseconds default argument");
