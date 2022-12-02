// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Verify the time zone arithmetic used in
  TemporalHelpers.crossDateLineTimeZone() against the Pacific/Apia time zone in
  the implementation's time zone database
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// No need to test this on hosts that don't provide an Intl object. It's
// sufficient that the logic is tested on at least one host.
if (typeof globalThis.Intl !== "undefined") {
  const tz = TemporalHelpers.crossDateLineTimeZone();
  const realTz = new Temporal.TimeZone('Pacific/Apia');
  const shiftInstant = Temporal.Instant.from('2011-12-30T10:00:00Z');

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

  const middleOfSkippedDayWallTime = Temporal.PlainDateTime.from("2011-12-30T12:00");
  assert.compareArray(
    tz.getPossibleInstantsFor(middleOfSkippedDayWallTime).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(middleOfSkippedDayWallTime).map((i) => i.epochNanoseconds),
    'possible instants for middle of skipped day wall time'
  );
  const before1 = middleOfSkippedDayWallTime.subtract({ days: 1 });
  assert.compareArray(
    tz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(before1).map((i) => i.epochNanoseconds),
    'possible instants for 1 day before skipped day'
  );
  const after1 = middleOfSkippedDayWallTime.add({ days: 1 });
  assert.compareArray(
    tz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    realTz.getPossibleInstantsFor(after1).map((i) => i.epochNanoseconds),
    'possible instants for 1 day after skipped day'
  );
}
