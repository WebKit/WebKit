// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.add
description: >
  Duration components are precise mathematical integers.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let duration = Temporal.Duration.from({
  hours: Number.MAX_VALUE,
  minutes: Number.MAX_VALUE,
  seconds: Number.MAX_VALUE,
  milliseconds: Number.MAX_VALUE,
  microseconds: Number.MAX_VALUE,
  nanoseconds: Number.MAX_VALUE,
});

let time = new Temporal.PlainTime(0, 0, 0, 0, 0, 0);

let result = time.add(duration);

TemporalHelpers.assertPlainTime(result, 3, 53, 35, 351, 226, 368);
