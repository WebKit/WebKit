// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: Mixed positive and negative values or missing properties always throw
features: [Temporal]
---*/

const duration = Temporal.Duration.from({ days: 1, minutes: 5 });
assert.throws(RangeError, () => duration.subtract({ hours: 1, minutes: -30 }), "mixed signs");
assert.throws(TypeError, () => duration.subtract({}), "no properties");
assert.throws(TypeError, () => duration.subtract({ month: 12 }), "only singular 'month' property");
