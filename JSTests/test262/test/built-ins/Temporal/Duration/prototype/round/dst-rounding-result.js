// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: >
    Rounding the resulting duration takes the time zone's UTC offset shifts
    into account
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const timeZone = TemporalHelpers.springForwardFallBackTimeZone();

// Based on a test case by Adam Shaw

{
  // Date part of duration lands on skipped DST hour, causing disambiguation
  const duration = new Temporal.Duration(0, 1, 0, 15, 12);
  const relativeTo = new Temporal.ZonedDateTime(
    950868000_000_000_000n /* = 2000-02-18T10Z */,
    timeZone); /* = 2000-02-18T02-08 in local time */

  TemporalHelpers.assertDuration(duration.round({ smallestUnit: "months", relativeTo }),
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    "1 month 15 days 12 hours should be exactly 1.5 months, which rounds up to 2 months");
  TemporalHelpers.assertDuration(duration.round({ smallestUnit: "months", roundingMode: 'halfTrunc', relativeTo }),
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    "1 month 15 days 12 hours should be exactly 1.5 months, which rounds down to 1 month");
}

{
  // Month-only part of duration lands on skipped DST hour, should not cause
  // disambiguation
  const duration = new Temporal.Duration(0, 1, 0, 15);
  const relativeTo = new Temporal.ZonedDateTime(
    951991200_000_000_000n /* = 2000-03-02T10Z */,
    timeZone); /* = 2000-03-02T02-08 in local time */

  TemporalHelpers.assertDuration(duration.round({ smallestUnit: "months", relativeTo }),
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    "1 month 15 days should be exactly 1.5 months, which rounds up to 2 months");
  TemporalHelpers.assertDuration(duration.round({ smallestUnit: "months", roundingMode: 'halfTrunc', relativeTo }),
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    "1 month 15 days should be exactly 1.5 months, which rounds down to 1 month");
}
