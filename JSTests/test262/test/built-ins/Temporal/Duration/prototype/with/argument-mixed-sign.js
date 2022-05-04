// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: The durationLike argument must not contain different signs.
features: [Temporal]
---*/

const d = new Temporal.Duration(1, 2, 3, 4, 5);
assert.throws(RangeError, () => d.with({ hours: 1, minutes: -1 }));
