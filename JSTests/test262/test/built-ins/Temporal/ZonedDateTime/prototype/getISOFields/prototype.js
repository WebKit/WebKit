// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: Correct prototype on the object returned from getISOFields
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1_000_086_400_987_654_321n, "UTC");
const result = instance.getISOFields();
assert.sameValue(Object.getPrototypeOf(result), Object.prototype, "prototype");
