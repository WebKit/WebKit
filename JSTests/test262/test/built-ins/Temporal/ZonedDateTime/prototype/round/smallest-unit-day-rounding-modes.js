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

  constructor(todayEpochNanoseconds, tomorrowEpochNanoseconds) {
    super("UTC");
    this.#nanoseconds = [todayEpochNanoseconds, tomorrowEpochNanoseconds];
  }
  getPossibleInstantsFor(dateTime) {
    const nanoseconds = this.#nanoseconds[this.#count++];
    if (nanoseconds === undefined) {
      return super.getPossibleInstantsFor(dateTime);
    }
    return [new Temporal.Instant(nanoseconds)];
  }
}

function test(epochNanoseconds, todayEpochNanoseconds, tomorrowEpochNanoseconds, testCases) {
  for (let [roundingMode, expected] of Object.entries(testCases)) {
    let timeZone = new TimeZone(todayEpochNanoseconds, tomorrowEpochNanoseconds);
    let zoned = new Temporal.ZonedDateTime(epochNanoseconds, timeZone);
    let result = zoned.round({ smallestUnit: "days", roundingMode });
    assert.sameValue(result.epochNanoseconds, expected);
  }
}

const oneDay = 24n * 60n * 60n * 1000n * 1000n * 1000n;

test(3n, undefined, 10n, {
  ceil: 10n, // end-of-day according to TimeZone protocol
  floor: 0n,
  trunc: 0n,
  halfExpand: 0n,
});

test(-3n, undefined, 10n, {
  ceil: 10n, // end-of-day according to TimeZone protocol
  floor: -oneDay,
  trunc: -oneDay,
  halfExpand: 10n, // end-of-day according to TimeZone protocol
});

assert.throws(RangeError, () => {
  test(-3n, 0n, 10n, { ceil: undefined });
}, "instant is before TimeZone protocol's start-of-day");

assert.throws(RangeError, () => {
  test(-3n, undefined, -10n, { ceil: undefined });
}, "instant is after TimeZone protocol's end-of-day");

assert.throws(RangeError, () => {
  test(0n, 0n, 0n, { ceil: undefined });
}, "instant is within zero-duration day");

// Test values at int64 boundaries.
test(3n, undefined, /*INT64_MAX=*/ 9223372036854775807n, {
  ceil: /*INT64_MAX=*/ 9223372036854775807n, // end-of-day according to TimeZone protocol
  floor: 0n,
  trunc: 0n,
  halfExpand: 0n,
});
