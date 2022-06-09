// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: Verify that undefined options are handled correctly.
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 987, 650);
const expected = "12:34:56.98765";

const explicit = time.toString(undefined);
assert.sameValue(explicit, expected, "default precision is auto and no rounding");

const propertyImplicit = time.toString({});
assert.sameValue(propertyImplicit, expected, "default precision is auto and no rounding");

const implicit = time.toString();
assert.sameValue(implicit, expected, "default precision is auto and no rounding");
