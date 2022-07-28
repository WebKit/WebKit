// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  Round smallestUnit "day" with very large or very small divisor (dayLengthNs).
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

const maxInstant = 86_40000_00000_00000_00000n;
const minInstant = -86_40000_00000_00000_00000n;
const oneDay = 24n * 60n * 60n * 1000n * 1000n * 1000n

// Divisor too large.
{
  let tz = new TimeZone(maxInstant);
  let zoned = new Temporal.ZonedDateTime(0n, tz);
  let result = zoned.round({ smallestUnit: "days" });
  assert(zoned.equals(result));
}
{
  let tz = new TimeZone(maxInstant);
  let zoned = new Temporal.ZonedDateTime(minInstant, tz);
  let result = zoned.round({ smallestUnit: "days" });
  assert(zoned.equals(result));
}

// Divisor too small.
{
  let tz = new TimeZone(minInstant);
  let zoned = new Temporal.ZonedDateTime(0n, tz);
  assert.throws(RangeError, () => zoned.round({ smallestUnit: "days" }));
}
{
  let tz = new TimeZone(minInstant);
  let zoned = new Temporal.ZonedDateTime(maxInstant - oneDay, tz);
  assert.throws(RangeError, () => zoned.round({ smallestUnit: "days" }));
}
