// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: Test for Temporal.TimeZone subclassing.
features: [Temporal]
---*/

class CustomTimeZone extends Temporal.TimeZone {
}

const instance = new CustomTimeZone("UTC");
assert.sameValue(instance.toString(), "UTC");
assert.sameValue(Object.getPrototypeOf(instance), CustomTimeZone.prototype, "Instance of CustomTimeZone");
assert(instance instanceof CustomTimeZone, "Instance of CustomTimeZone");
assert(instance instanceof Temporal.TimeZone, "Instance of Temporal.TimeZone");
