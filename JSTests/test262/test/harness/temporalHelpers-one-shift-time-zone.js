// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Verify the time zone arithmetic used in TemporalHelpers.oneShiftTimeZone()
  against known cases in the implementation's time zone database
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

function checkTimeZoneArithmetic(shiftInstant, shiftNs, realTimeZoneName, shiftWallTime) {
  // No need to test this on hosts that don't provide an Intl object. It's
  // sufficient that the logic is tested on at least one host.
  if (typeof globalThis.Intl === "undefined")
    return;

  const tz = TemporalHelpers.oneShiftTimeZone(shiftInstant, shiftNs);
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
    tz.getPossibleInstantsFor(shiftWallTime).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(shiftWallTime).map((i) => i.epochNanoseconds),
    'possible instants for wall time'
  );
  const before1 = shiftWallTime.subtract({ hours: 1 });
  assert.compareArray(
    tz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    'possible instants for 1 hour before wall time'
  );
  const after1 = shiftWallTime.add({ hours: 1 });
  assert.compareArray(
    tz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    'possible instants for 1 hour after wall time'
  );
}

// Check a positive DST shift from +00:00 to +01:00
checkTimeZoneArithmetic(
  new Temporal.Instant(1616893200000000000n),
  3600e9,
  'Europe/London',
  new Temporal.PlainDateTime(2021, 3, 28, 1)
);

// Check a negative DST shift from +00:00 to -01:00
checkTimeZoneArithmetic(
  new Temporal.Instant(1635642000000000000n),
  -3600e9,
  'Atlantic/Azores',
  new Temporal.PlainDateTime(2021, 10, 31, 1)
);

// Check the no-shift case
checkTimeZoneArithmetic(
  new Temporal.Instant(0n),
  0,
  'UTC',
  new Temporal.PlainDateTime(1970, 1, 1)
);
