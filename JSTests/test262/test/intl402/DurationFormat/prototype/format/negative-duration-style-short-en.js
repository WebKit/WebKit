// Copyright (C) 2023 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype.format
description: >
  Test format method with negative duration and "short" style
locale: [en]
includes: [testIntl.js]
features: [Intl.DurationFormat]
---*/

const style = "short";

const duration = {
  years: -1,
  months: -2,
  weeks: -3,
  days: -3,
  hours: -4,
  minutes: -5,
  seconds: -6,
  milliseconds: -7,
  microseconds: -8,
  nanoseconds: -9,
};

const expected = formatDurationFormatPattern(duration, style);

const df = new Intl.DurationFormat("en", {style});
assert.sameValue(
  df.format(duration),
  expected,
  `DurationFormat format output using ${style} style option`
);
