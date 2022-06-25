// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: Singular properties are ignored.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const d = Temporal.Duration.from({ years: 5, days: 1 });
const d2 = d.with({ year: 1, days: 0 });
TemporalHelpers.assertDuration(d2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0);
