// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Duration(0, 0, 0, 0, 1);

const result1 = instance.add({ hours: 1 }, {});
TemporalHelpers.assertDuration(
  result1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
  "options may be an empty plain object"
);

const result2 = instance.add({ hours: 1 }, () => {});
TemporalHelpers.assertDuration(
  result2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
  "options may be a function object"
);
