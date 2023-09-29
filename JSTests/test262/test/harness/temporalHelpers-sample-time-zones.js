// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Verify the time zone arithmetic used in TemporalHelpers.oneShiftTimeZone()
  against known cases in the implementation's time zone database
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

function checkTimeZoneArithmetic(shiftInstant, testWallTime, testWallDuration, tz, realTimeZoneName) {
  // No need to test this on hosts that don't provide an Intl object. It's
  // sufficient that the logic is tested on at least one host.
  if (typeof globalThis.Intl === "undefined")
    return;

  const realTz = new Temporal.TimeZone(realTimeZoneName);

  assert.sameValue(
    tz.getOffsetNanosecondsFor(shiftInstant),
    realTz.getOffsetNanosecondsFor(shiftInstant),
    'offset at shift instant'
  );
  const minus1 = shiftInstant.subtract({ hours: 1 });
  assert.sameValue(
    tz.getOffsetNanosecondsFor(minus1),
    realTz.getOffsetNanosecondsFor(minus1),
    'offset at 1 hour before shift'
  );
  const plus1 = shiftInstant.add({ hours: 1 });
  assert.sameValue(
    tz.getOffsetNanosecondsFor(plus1),
    realTz.getOffsetNanosecondsFor(plus1),
    'offset at 1 hour after shift'
  );

  assert.compareArray(
    tz.getPossibleInstantsFor(testWallTime).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(testWallTime).map((i) => i.epochNanoseconds),
    'possible instants for wall time'
  );
  const before1 = testWallTime.subtract(testWallDuration);
  assert.compareArray(
    tz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    'possible instants before wall time'
  );
  const after1 = testWallTime.add(testWallDuration);
  assert.compareArray(
    tz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    'possible instants after wall time'
  );
}

// Check a positive DST shift from +00:00 to +01:00
checkTimeZoneArithmetic(
  new Temporal.Instant(1616893200000000000n),
  new Temporal.PlainDateTime(2021, 3, 28, 1),
  { hours: 1 },
  TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(1616893200000000000n), 3600e9),
  'Europe/London',
);

// Check a negative DST shift from +00:00 to -01:00
checkTimeZoneArithmetic(
  new Temporal.Instant(1635642000000000000n),
  new Temporal.PlainDateTime(2021, 10, 31, 1),
  { hours: 1 },
  TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(1635642000000000000n), -3600e9),
  'Atlantic/Azores',
);

// Check the no-shift case
checkTimeZoneArithmetic(
  new Temporal.Instant(0n),
  new Temporal.PlainDateTime(1970, 1, 1),
  { hours: 1 },
  TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 0),
  'UTC',
);

// Check the cross-date-line sample time zone
checkTimeZoneArithmetic(
  Temporal.Instant.from('2011-12-30T10:00:00Z'),
  Temporal.PlainDateTime.from("2011-12-30T12:00"),
  { days: 1 },
  TemporalHelpers.crossDateLineTimeZone(),
  'Pacific/Apia',
);

// Check the spring-forward transition of the DST sample time zone
checkTimeZoneArithmetic(
  new Temporal.Instant(954669600_000_000_000n),
  new Temporal.PlainDateTime(2000, 4, 2, 2),
  { minutes: 30 },
  TemporalHelpers.springForwardFallBackTimeZone(),
  'America/Vancouver',
);

// Check the fall-back transition of the DST sample time zone
checkTimeZoneArithmetic(
  new Temporal.Instant(972810000_000_000_000n),
  new Temporal.PlainDateTime(2000, 10, 29, 2),
  { minutes: 30 },
  TemporalHelpers.springForwardFallBackTimeZone(),
  'America/Vancouver',
);
