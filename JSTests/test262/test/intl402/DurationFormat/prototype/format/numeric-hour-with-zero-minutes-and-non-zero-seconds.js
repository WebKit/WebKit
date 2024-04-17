// Copyright (C) 2023 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype.format
description: >
  Minutes with numeric or 2-digit style are included in the output when between displayed hours and seconds, even when the minutes value is zero.
locale: [en]
features: [Intl.DurationFormat]
---*/

const df = new Intl.DurationFormat("en", {
  // hours must be numeric, so that a time separator is used for the following units.
  hours: "numeric",
});

const duration = {
  hours: 1,
  minutes: 0,
  seconds: 3,
};

const expected = "1:00:03"

assert.sameValue(
  df.format(duration),
  expected,
  `Minutes always displayed when between displayed hours and seconds, even if minutes is 0`
);
