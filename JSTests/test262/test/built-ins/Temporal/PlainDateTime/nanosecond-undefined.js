// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Nanosecond argument defaults to 0 if not given
features: [Temporal]
---*/

const args = [2000, 5, 2, 12, 34, 56, 123, 456];

const explicit = new Temporal.PlainDateTime(...args, undefined);
assert.sameValue(explicit.nanosecond, 0, "nanosecond default argument");

const implicit = new Temporal.PlainDateTime(...args);
assert.sameValue(implicit.nanosecond, 0, "nanosecond default argument");
