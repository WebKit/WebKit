// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: >
  Throws when at minimum resp. maximum value and possible instants is an empty List.
info: |
  DisambiguatePossibleInstants ( possibleInstants, timeZone, dateTime, disambiguation )

  ...
  9. If ! IsValidEpochNanoseconds(dayBeforeNs) is false, throw a RangeError exception.
  ...
  12. If ! IsValidEpochNanoseconds(dayAfterNs) is false, throw a RangeError exception.
  ...
features: [Temporal]
---*/

class TZ extends Temporal.TimeZone {
  getPossibleInstantsFor() {
    return [];
  }
}

var tz = new TZ("UTC");
var min = new Temporal.PlainDateTime(-271821, 4, 20);
var max = new Temporal.PlainDateTime(275760, 9, 13);

assert.throws(RangeError, () => min.toZonedDateTime(tz), "minimum date-time");
assert.throws(RangeError, () => max.toZonedDateTime(tz), "maximum date-time");
