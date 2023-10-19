// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.subtract
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

const dayNs = 86_400_000_000_000;
const dayDuration = Temporal.Duration.from({ days: 1 });
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
  -1n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 9
      [epochInstant], // Returned for AddDuration step 10, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [epochInstant], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    ],
    [
      // Behave normally in 3 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);

// NanosecondsToDays.20: days > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  1n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 9
      [epochInstant], // Returned for AddDuration step 10, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [epochInstant], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
    ],
    [
      // Behave normally in 3 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      -dayNs + 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      dayNs - 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);

// NanosecondsToDays.22: nanoseconds > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 9
      [new Temporal.Instant(-1n)], // Returned for AddDuration step 10, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [new Temporal.Instant(-2n)], // Returned for NanosecondsToDays step 14, setting _intermediateNs_
      [new Temporal.Instant(-4n)], // Returned for NanosecondsToDays step 18.a, setting _oneDayFartherNs_
    ],
    [
      // Behave normally in 3 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned for NanosecondsToDays step 7, setting _startDateTime_
      -dayNs + 1, // Returned for NanosecondsToDays step 11, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);
