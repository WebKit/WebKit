// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.getisofields
description: Correct prototype on the object returned from getISOFields
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
const result = instance.getISOFields();
assert.sameValue(Object.getPrototypeOf(result), Object.prototype, "prototype");
