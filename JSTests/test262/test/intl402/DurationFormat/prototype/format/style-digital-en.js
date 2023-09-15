// Copyright 2022 Igalia, S.L. All rights reserved.
// Copyright 2023 Apple Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype.format
description: Test if format method formats duration correctly with different "style" arguments
locale: [en-US]
features: [Intl.DurationFormat]
---*/

const style = "digital";
const expected = "1 yr, 2 mths, 3 wks, 3 days, 4:05:06";

const duration = {
  years: 1,
  months: 2,
  weeks: 3,
  days: 3,
  hours: 4,
  minutes: 5,
  seconds: 6,
  milliseconds: 7,
  microseconds: 8,
  nanoseconds: 9,
};

const df = new Intl.DurationFormat("en", {style});
assert.sameValue(df.format(duration), expected, `Assert DurationFormat format output using ${style} style option`);
