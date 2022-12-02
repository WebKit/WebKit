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
    months: Number.MAX_SAFE_INTEGER,
    days: 31 + 28 + 31,
  });

  let result = duration.round({
    largestUnit: "months",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    0, 9007199254740994, 0, 0,
    0, 0, 0,
    0, 0, 0,
  );
}

{
  let date = new Temporal.PlainDate(1970, 1, 1);

  let duration = Temporal.Duration.from({
    months: Number.MAX_SAFE_INTEGER,
    days: 31 + 28 + 31 + 1,
  });

  let result = duration.round({
    largestUnit: "months",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    0, 9007199254740994, 0, 1,
    0, 0, 0,
    0, 0, 0,
  );
}
