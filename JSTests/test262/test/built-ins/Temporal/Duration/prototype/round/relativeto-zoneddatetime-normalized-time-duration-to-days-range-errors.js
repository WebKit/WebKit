// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: >
  Abstract operation NormalizedTimeDurationToDays can throw four different
  RangeErrors.
info: |
  NormalizedTimeDurationToDays ( norm, zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
    23. If days < 0 and sign = 1, throw a RangeError exception.
    24. If days > 0 and sign = -1, throw a RangeError exception.
    ...
    26. If NormalizedTimeDurationSign(_norm_) = 1 and sign = -1, throw a RangeError exception.
    ...
    29. If dayLength ≥ 2⁵³, throw a RangeError exception.
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

// Step 23: days < 0 and sign = 1
let zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned in step 16, setting _relativeResult_
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      dayNs - 1, // Returned in step 8, setting _startDateTime_
      -dayNs + 1, // Returned in step 9, setting _endDateTime_
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

// Step 24: days > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned in step 16, setting _relativeResult_
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      -dayNs + 1, // Returned in step 8, setting _startDateTime_
      dayNs - 1, // Returned in step 9, setting _endDateTime_
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

// Step 26: nanoseconds > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Sets _startNs_ to 0
  timeZoneSubstituteValues(
    [
      [new Temporal.Instant(-2n)], // Returned in step 16, setting _relativeResult_
      [new Temporal.Instant(-4n)], // Returned in step 19, setting _oneDayFarther_
    ],
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Pre-conversion in Duration.p.round
      dayNs - 1, // Returned in step 8, setting _startDateTime_
      -dayNs + 1, // Returned in step 9, setting _endDateTime_
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

// Step 29: day length is an unsafe integer
zdt = new Temporal.ZonedDateTime(
  0n,
  timeZoneSubstituteValues(
    // Not called in step 16 because _days_ = 0
    // Returned in step 19, making _oneDayFarther_ 2^53 ns later than _relativeResult_
    [[new Temporal.Instant(2n ** 53n)]],
    []
  )
);
assert.throws(RangeError, () =>
  oneNsDuration.round({
    relativeTo: zdt,
    smallestUnit: "days",
  }),
  "Should throw RangeError when time zone calculates an outrageous day length"
);
