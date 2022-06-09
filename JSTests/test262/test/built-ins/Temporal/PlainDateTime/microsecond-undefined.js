// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Microsecond argument defaults to 0 if not given
features: [Temporal]
---*/

const args = [2000, 5, 2, 12, 34, 56, 123];

const explicit = new Temporal.PlainDateTime(...args, undefined);
assert.sameValue(explicit.microsecond, 0, "microsecond default argument");

const implicit = new Temporal.PlainDateTime(...args);
assert.sameValue(implicit.microsecond, 0, "microsecond default argument");
