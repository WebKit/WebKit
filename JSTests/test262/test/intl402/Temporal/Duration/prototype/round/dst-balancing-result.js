// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: >
    Balancing the resulting duration takes the time zone's UTC offset shifts
    into account
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// Based on a test case by Adam Shaw
{
    const duration = new Temporal.Duration(1, 0, 0, 0, 24);
    const relativeTo = new Temporal.ZonedDateTime(
        941184000_000_000_000n /* = 1999-10-29T08Z */,
        "America/Vancouver"); /* = 1999-10-29T00-08 in local time */

    const result = duration.round({ largestUnit: "years", relativeTo });
    TemporalHelpers.assertDuration(result, 1, 0, 0, 0, 24, 0, 0, 0, 0, 0,
        "24 hours does not balance to 1 day in 25-hour day");
}

{
  const duration = new Temporal.Duration(0, 0, 0, 0, /* hours = */ 24, 0, 0, 0, 0, /* ns = */ 5);
  const relativeTo = new Temporal.ZonedDateTime(
    972802800_000_000_000n /* = 2000-10-29T07Z */,
    "America/Vancouver"); /* = 2000-10-29T00-07 in local time */
  
  const result = duration.round({
    largestUnit: "days",
    smallestUnit: "minutes",
    roundingMode: "expand",
    roundingIncrement: 30,
    relativeTo
  });
  TemporalHelpers.assertDuration(result, 0, 0, 0, 0, 24, 30, 0, 0, 0, 0,
    "24 hours does not balance after rounding to 1 day in 25-hour day");
}
