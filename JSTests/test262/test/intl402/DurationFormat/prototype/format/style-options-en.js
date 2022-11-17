// Copyright 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype.format
description: Test if format method formats duration correctly with different "style" arguments
locale: [en-US]
features: [Intl.DurationFormat]
---*/

const testData = {
  "long" : "1 year, 2 months, 3 weeks, 3 days, 4 hours, 5 minutes, 6 seconds, 7 milliseconds, 8 microseconds, 9 nanoseconds",
  "short": "1 yr, 2 mths, 3 wks, 3 days, 4 hr, 5 min, 6 sec, 7 ms, 8 μs, 9 ns",
  "narrow":"1y 2m 3w 3d 4h 5m 6s 7ms 8μs 9ns",
  "digital":"1 yr 2 mths 3 wks 3 days 4:05:06",
}

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


for (const style in testData) {
   const df = new Intl.DurationFormat("en", {style});
   const expected = testData[style];
   assert.sameValue(df.format(duration), expected, `Assert DurationFormat format output using ${style} style option`);
}

