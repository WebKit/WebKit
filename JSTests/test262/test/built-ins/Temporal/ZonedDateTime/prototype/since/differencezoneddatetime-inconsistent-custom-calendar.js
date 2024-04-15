// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: >
  Throws a RangeError when custom calendar method returns inconsistent result
info: |
  DifferenceZonedDateTime ( ... )
  8. Repeat 3 times:
    ...
    g. If _sign_ = 0, or _timeSign_ = 0, or _sign_ = _timeSign_, then
      ...
      viii. Return ? CreateNormalizedDurationRecord(_dateDifference_.[[Years]],
            _dateDifference_.[[Months]], _dateDifference_.[[Weeks]],
            _dateDifference_.[[Days]], _norm_).
    h. Set _dayCorrection_ to _dayCorrection_ + 1.
  9. NOTE: This step is only reached when custom calendar or time zone methods
     return inconsistent values.
  10. Throw a *RangeError* exception.
features: [Temporal]
---*/

// Based partly on a test case by André Bargull

const fiftyDays12Hours = 50n * 86400_000_000_000n + 12n * 3600_000_000_000n;

{
  const tz = new (class extends Temporal.TimeZone {
    getPossibleInstantsFor(dateTime) {
      return super.getPossibleInstantsFor(dateTime.subtract({ days: 3 }));
    }
  })("UTC");

  const zdt1 = new Temporal.ZonedDateTime(0n, tz);
  const zdt2 = new Temporal.ZonedDateTime(fiftyDays12Hours, tz);

  assert.throws(RangeError, () => zdt2.since(zdt1, { largestUnit: "weeks" }),
    "Calendar calculation where more than 2 days correction is needed should cause RangeError");
}

{
  const cal = new (class extends Temporal.Calendar {
    dateUntil(one, two, options) {
      return super.dateUntil(one, two, options).negated();
    }
  })("iso8601");

  const zdt1 = new Temporal.ZonedDateTime(0n, "UTC", cal);
  const zdt2 = new Temporal.ZonedDateTime(fiftyDays12Hours, "UTC", cal);

  assert.throws(RangeError, () => zdt2.since(zdt1, { largestUnit: "weeks" }),
    "Calendar calculation causing mixed-sign values should cause RangeError");
}
