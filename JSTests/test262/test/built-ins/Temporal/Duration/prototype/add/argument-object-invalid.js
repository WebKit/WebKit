// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Mixed positive and negative values or missing properties always throw
features: [Temporal]
---*/

const duration = Temporal.Duration.from({ days: 1, minutes: 5 });
assert.throws(RangeError, () => duration.add({ hours: 1, minutes: -30 }), "mixed signs");
assert.throws(TypeError, () => duration.add({}), "no properties");
assert.throws(TypeError, () => duration.add({ month: 12 }), "only singular 'month' property");
