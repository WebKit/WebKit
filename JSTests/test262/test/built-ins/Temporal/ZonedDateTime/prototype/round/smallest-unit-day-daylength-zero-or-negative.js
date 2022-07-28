// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  Round smallestUnit "day" with zero or negative day length.
info: |
  Temporal.ZonedDateTime.prototype.round ( roundTo )
  ...
  18. Let dayLengthNs be ℝ(endNs - startNs).
  19. If dayLengthNs ≤ 0, then
    a. Throw a RangeError exception.
  20. Let roundResult be ! RoundISODateTime(temporalDateTime.[[ISOYear]],
      temporalDateTime.[[ISOMonth]], temporalDateTime.[[ISODay]], temporalDateTime.[[ISOHour]],
      temporalDateTime.[[ISOMinute]], temporalDateTime.[[ISOSecond]],
      temporalDateTime.[[ISOMillisecond]], temporalDateTime.[[ISOMicrosecond]],
      temporalDateTime.[[ISONanosecond]], roundingIncrement, smallestUnit, roundingMode,
      dayLengthNs).
  ...

  RoundISODateTime ( year, month, day, hour, minute, second, millisecond, microsecond, nanosecond,
                     increment, unit, roundingMode [ , dayLength ] )
  ...
  4. Let roundedTime be ! RoundTime(hour, minute, second, millisecond, microsecond, nanosecond,
     increment, unit, roundingMode, dayLength).
  ...

  RoundTime ( hour, minute, second, millisecond, microsecond, nanosecond, increment, unit,
              roundingMode [ , dayLengthNs ] )
  ...
  4. If unit is "day", then
    ...
    b. Let quantity be (((((hour × 60 + minute) × 60 + second) × 1000 + millisecond) × 1000 +
       microsecond) × 1000 + nanosecond) / dayLengthNs.
  ...
features: [Temporal]
---*/

class TimeZone extends Temporal.TimeZone {
  #count = 0;
  #nanoseconds;

  constructor(nanoseconds) {
    super("UTC");
    this.#nanoseconds = nanoseconds;
  }
  getPossibleInstantsFor(dateTime) {
    if (++this.#count === 2) {
      return [new Temporal.Instant(this.#nanoseconds)];
    }
    return super.getPossibleInstantsFor(dateTime);
  }
}

{
  let tz = new TimeZone(0n);
  let zoned = new Temporal.ZonedDateTime(0n, tz);
  assert.throws(RangeError, () => zoned.round({ smallestUnit: "days" }));
}

{
  let tz = new TimeZone(-1n);
  let zoned = new Temporal.ZonedDateTime(0n, tz);
  assert.throws(RangeError, () => zoned.round({ smallestUnit: "days" }));
}
