// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.subtract
description: >
  Duration components are precise mathematical integers.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let duration = Temporal.Duration.from({
  hours: -Number.MAX_VALUE,
  minutes: -Number.MAX_VALUE,
});

let time = new Temporal.PlainTime(0, 0, 0, 0, 0, 0);

let result = time.subtract(duration);

TemporalHelpers.assertPlainTime(result, 10, 8, 0, 0, 0, 0);
