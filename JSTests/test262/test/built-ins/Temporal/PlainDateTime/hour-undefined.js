// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Hour argument defaults to 0 if not given
features: [Temporal]
---*/

const args = [2000, 5, 2];

const explicit = new Temporal.PlainDateTime(...args, undefined);
assert.sameValue(explicit.hour, 0, "hour default argument");

const implicit = new Temporal.PlainDateTime(...args);
assert.sameValue(implicit.hour, 0, "hour default argument");
