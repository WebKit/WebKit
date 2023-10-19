// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: >
  Called abstract operation NanosecondsToDays can throw three different RangeErrors when paired with a ZonedDateTime.
info: |
  6.5.7 NanosecondsToDays ( nanoseconds, relativeTo )
    19. If days < 0 and sign = 1, throw a RangeError exception.
    20. If days > 0 and sign = -1, throw a RangeError exception.
    ...
    22. If nanoseconds > 0 and sign = -1, throw a RangeError exception.
features: [Temporal, BigInt]
includes: [temporalHelpers.js]
---*/

const oneNsDuration = Temporal.Duration.from({ nanoseconds: 1 });
const negOneNsDuration = Temporal.Duration.from({ nanoseconds: -1 });
const dayNs = 86_400_000_000_000;
const epochInstant = new Temporal.Instant(0n);

function timeZoneSubstituteValues(
  getPossibleInstantsFor,
  getOffsetNanosecondsFor
) {
  const tz = new Temporal.TimeZone("UTC");
  TemporalHelpers.substituteMethod(
    tz,
    "getPossibleInstantsFor",
    getPossibleInstantsFor
  );
  TemporalHelpers.substituteMethod(
    tz,
    "getOffsetNanosecondsFor",
    getOffsetNanosecondsFor
  );
  return tz;
}

// NanosecondsToDays.19: days < 0 and sign = 1
let zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Using 1ns duration sets _nanoseconds_ to 1 and _sign_ to 1
  oneNsDuration.round({
    relativeTo: zdt,
    smallestUnit: "days",
  }),
  "RangeError when days < 0 and sign = 1"
);

// NanosecondsToDays.20: days > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      -dayNs + 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      dayNs - 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Using -1ns duration sets _nanoseconds_ to -1 and _sign_ to -1
  negOneNsDuration.round({
    relativeTo: zdt,
    smallestUnit: "days",
  }),
  "RangeError when days > 0 and sign = -1"
);

// NanosecondsToDays.22: nanoseconds > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [
      [new Temporal.Instant(-2n)], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
      [new Temporal.Instant(-4n)], // Returned for NanosecondsToDays step 18.a, setting _oneDayFartherNs_
    ],
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Using -1ns duration sets _nanoseconds_ to -1 and _sign_ to -1
  negOneNsDuration.round({
    relativeTo: zdt,
    smallestUnit: "days",
  }),
  "RangeError when nanoseconds > 0 and sign = -1"
);
