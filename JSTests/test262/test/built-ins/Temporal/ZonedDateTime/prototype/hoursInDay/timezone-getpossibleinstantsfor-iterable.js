// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.hoursinday
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-get-temporal.zoneddatetime.prototype.hoursinday steps 13–14:
      13. Let _todayInstant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _today_, *"compatible"*).
      14. Let _tomorrowInstant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _tomorrow_, *"compatible"*).
    sec-temporal-builtintimezonegetinstantfor step 1:
      1. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-builtintimezonegetinstantfor step 14:
      14. Assert: _disambiguation_ is *"compatible"* or *"later"*.
    sec-temporal-builtintimezonegetinstantfor step 16:
      16. Set _possibleInstants_ to ? GetPossibleInstantsFor(_timeZone_, _later_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected1 = [
  "2001-09-09T00:00:00",
  "2001-09-10T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);
  datetime.hoursInDay;
}, expected1);

// Same, but test the other path where the time doesn't exist and
// GetPossibleInstantsFor is called again on a later time

const expected2 = [
  "2030-01-01T00:00:00",
  "2030-01-01T01:00:00",
  "2030-01-02T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_893_457_800_000_000_000n, timeZone);
  datetime.hoursInDay;
}, expected2);
