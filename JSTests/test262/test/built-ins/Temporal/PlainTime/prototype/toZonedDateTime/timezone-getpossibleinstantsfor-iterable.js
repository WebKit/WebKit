// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.plaintime.prototype.tozoneddatetime step 10:
      10. Let _instant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _temporalDateTime_, *"compatible"*).
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
  "2000-05-02T12:34:56.987654321",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
  time.toZonedDateTime({ plainDate: new Temporal.PlainDate(2000, 5, 2), timeZone });
}, expected1);

// Same, but test the other path where the time doesn't exist and
// GetPossibleInstantsFor is called again on a later time

const expected2 = [
  "2030-01-01T00:30:00",
  "2030-01-01T01:30:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const time = new Temporal.PlainTime(0, 30);
  time.toZonedDateTime({ plainDate: new Temporal.PlainDate(2030, 1, 1), timeZone });
}, expected2);
