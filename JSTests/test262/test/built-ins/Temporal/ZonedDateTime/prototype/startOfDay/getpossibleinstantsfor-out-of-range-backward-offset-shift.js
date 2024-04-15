// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.startofday
description: >
  UTC offset shift returned by getPossibleInstantsFor can be at most 24 hours.
features: [Temporal]
info: |
  GetPossibleInstantsFor:
  5.b.i. Let _numResults_ be _list_'s length.
  ii. If _numResults_ > 1, then
    1. Let _epochNs_ be a new empty List.
    2. For each value _instant_ in list, do
      a. Append _instant_.[[EpochNanoseconds]] to the end of the List _epochNs_.
    3. Let _min_ be the least element of the List _epochNs_.
    4. Let _max_ be the greatest element of the List _epochNs_.
    5. If abs(ℝ(_max_ - _min_)) > nsPerDay, throw a *RangeError* exception.
---*/

class ShiftLonger24Hour extends Temporal.TimeZone {
  id = 'TestTimeZone';

  constructor() {
    super('UTC');
  }

  getOffsetNanosecondsFor(instant) {
    return 0;
  }

  getPossibleInstantsFor(plainDateTime) {
    const utc = new Temporal.TimeZone("UTC");
    const [utcInstant] = utc.getPossibleInstantsFor(plainDateTime);
    return [
      utcInstant.subtract({ hours: 12, nanoseconds: 1 }),
      utcInstant.add({ hours: 12 }),
      utcInstant, // add a third value in case the implementation doesn't sort
    ];
  }
}

const timeZone = new ShiftLonger24Hour();

const instance = new Temporal.ZonedDateTime(0n, timeZone);
assert.throws(RangeError, () => instance.startOfDay(), "RangeError should be thrown");
