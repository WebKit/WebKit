// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  Round smallestUnit "day" with various rounding modes.
info: |
  Temporal.ZonedDateTime.prototype.round ( roundTo )
  ...
  18. Let dayLengthNs be ℝ(endNs - startNs).
  ...
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

function test(epochNanoseconds, tomorrowEpochNanoseconds, testCases) {
  for (let [roundingMode, expected] of Object.entries(testCases)) {
    let timeZone = new TimeZone(tomorrowEpochNanoseconds);
    let zoned = new Temporal.ZonedDateTime(epochNanoseconds, timeZone);
    let result = zoned.round({ smallestUnit: "days", roundingMode });
    assert.sameValue(result.epochNanoseconds, expected);
  }
}

const oneDay = 24n * 60n * 60n * 1000n * 1000n * 1000n;

// Test positive divisor (dayLengthNs).
test(3n, 10n, {
  ceil: oneDay,
  floor: 0n,
  trunc: 0n,
  halfExpand: 0n,
});

test(-3n, 10n, {
  ceil: 0n,
  floor: -oneDay,
  trunc: -oneDay,
  halfExpand: 0n,
});

test(-3n, -10n, {
  ceil: oneDay,
  floor: 0n,
  trunc: 0n,
  halfExpand: 0n,
});

// Test values at int64 boundaries.
test(3n, /*INT64_MAX=*/ 9223372036854775807n, {
  ceil: oneDay,
  floor: 0n,
  trunc: 0n,
  halfExpand: 0n,
});
