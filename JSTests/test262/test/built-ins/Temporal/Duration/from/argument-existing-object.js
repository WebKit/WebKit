// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.from
description: Property bag is converted to Duration; Duration is copied
features: [Temporal]
---*/

const d1 = Temporal.Duration.from({ milliseconds: 1000 });
assert.sameValue(d1.seconds, 0);
assert.sameValue(d1.milliseconds, 1000);

const d2 = Temporal.Duration.from(d1);
assert.notSameValue(d1, d2);
assert.sameValue(d1.seconds, 0);
assert.sameValue(d1.milliseconds, 1000);
assert.sameValue(d2.seconds, 0);
assert.sameValue(d2.milliseconds, 1000);
