// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.tostring
description: Temporal.Duration.toString handles negative components
features: [Temporal]
---*/
const d = new Temporal.Duration(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const expected = "-P1Y1M1W1DT1H1M1.001001001S";
assert.sameValue(d.toString(), expected, "toString with negative components");
