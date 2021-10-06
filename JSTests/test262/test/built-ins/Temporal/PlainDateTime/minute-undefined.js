// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Minute argument defaults to 0 if not given
features: [Temporal]
---*/

const args = [2000, 5, 2, 12];

const explicit = new Temporal.PlainDateTime(...args, undefined);
assert.sameValue(explicit.minute, 0, "minute default argument");

const implicit = new Temporal.PlainDateTime(...args);
assert.sameValue(implicit.minute, 0, "minute default argument");
