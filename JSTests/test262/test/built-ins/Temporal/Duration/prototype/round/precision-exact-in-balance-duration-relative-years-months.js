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
    years: Number.MAX_SAFE_INTEGER,
    months: 12 * 3,
  });

  let result = duration.round({
    largestUnit: "years",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    9007199254740994, 0, 0, 0,
    0, 0, 0,
    0, 0, 0,
  );
}

{
  let date = new Temporal.PlainDate(1970, 1, 1);

  let duration = Temporal.Duration.from({
    years: Number.MAX_SAFE_INTEGER,
    months: 12 * 3 + 1,
  });

  let result = duration.round({
    largestUnit: "years",
    relativeTo: date,
  });

  TemporalHelpers.assertDuration(
    result,
    9007199254740994, 1, 0, 0,
    0, 0, 0,
    0, 0, 0,
  );
}
