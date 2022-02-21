// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration
description: Undefined arguments should be treated as zero.
features: [Temporal]
---*/

const explicit = new Temporal.Duration(undefined);
assert.sameValue(explicit.years, 0, "years default argument");

const implicit = new Temporal.Duration();
assert.sameValue(implicit.years, 0, "years default argument");
