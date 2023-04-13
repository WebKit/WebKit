// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.since
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

const dayNs = 86_400_000_000_000;
const zeroZDT = new Temporal.ZonedDateTime(0n, "UTC");
const oneZDT = new Temporal.ZonedDateTime(1n, "UTC");
const epochInstant = new Temporal.Instant(0n);
const options = { largestUnit: "days" };

// NanosecondsToDays.19: days < 0 and sign = 1
let start = new Temporal.ZonedDateTime(
  0n, // Sets DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    [
      // Behave normally in 2 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  start.since(
    oneZDT, // Sets DifferenceZonedDateTime _ns2_
    options
  )
);

// NanosecondsToDays.20: days > 0 and sign = -1
start = new Temporal.ZonedDateTime(
  1n, // Sets DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [[epochInstant]], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    [
      // Behave normally in 2 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      -dayNs + 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      dayNs - 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  start.since(
    zeroZDT, // Sets DifferenceZonedDateTime _ns2_
    options
  )
);

// NanosecondsToDays.22: nanoseconds > 0 and sign = -1
start = new Temporal.ZonedDateTime(
  1n, // Sets DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [[new Temporal.Instant(-1n)]], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    [
      // Behave normally in 2 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  start.since(
    zeroZDT, // Sets DifferenceZonedDateTime _ns2_
    options
  )
);
