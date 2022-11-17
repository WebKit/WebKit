// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  BalanceDurationRelative computes on exact mathematical values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

{
  let date = new Temporal.PlainDate(1970, 1, 1);

  let duration = Temporal.Duration.from({
    weeks: Number.MAX_SAFE_INTEGER,
    days: 7 * 3,
  });

  let result = duration.round({
    largestUnit: "weeks",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    0, 0, 9007199254740994, 0,
    0, 0, 0,
    0, 0, 0,
  );
}

{
  let date = new Temporal.PlainDate(1970, 1, 1);

  let duration = Temporal.Duration.from({
    weeks: Number.MAX_SAFE_INTEGER,
    days: 7 * 3 + 1,
  });

  let result = duration.round({
    largestUnit: "weeks",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    0, 0, 9007199254740994, 1,
    0, 0, 0,
    0, 0, 0,
  );
}
