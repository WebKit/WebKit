// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: TimeZone.from() with invalid objects.
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.TimeZone.from({ timeZone: "local" }));
assert.throws(RangeError, () => Temporal.TimeZone.from({ timeZone: { timeZone: "UTC" } }));
