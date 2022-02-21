// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getnexttransition
description: Negative zero, as an extended year, is rejected
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

let str = "-000000-01-01T00:00";
assert.throws(RangeError, () => instance.getNextTransition(str), "reject minus zero as extended year");
