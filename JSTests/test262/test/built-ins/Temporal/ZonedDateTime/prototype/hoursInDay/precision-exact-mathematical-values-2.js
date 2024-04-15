// Copyright (C) 2024 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.hoursinday
description: >
  Hours in day is correctly rounded using precise mathematical values.
info: |
  get Temporal.ZonedDateTime.prototype.hoursInDay

  ...
  14. Let diffNs be tomorrowInstant.[[Nanoseconds]] - todayInstant.[[Nanoseconds]].
  15. Return 𝔽(diffNs / (3.6 × 10^12)).
features: [Temporal]
---*/

// Randomly generated test data.
const data = [
  {
    hours: 816,
    nanoseconds: 2049_187_497_660,
  },
  {
    hours: 7825,
    nanoseconds: 1865_665_040_770,
  },
  {
    hours: 0,
    nanoseconds: 1049_560_584_034,
  },
  {
    hours: 2055144,
    nanoseconds: 2502_078_444_371,
  },
  {
    hours: 31,
    nanoseconds: 1010_734_758_745,
  },
  {
    hours: 24,
    nanoseconds: 2958_999_560_387,
  },
  {
    hours: 0,
    nanoseconds: 342_058_521_588,
  },
  {
    hours: 17746,
    nanoseconds: 3009_093_506_309,
  },
  {
    hours: 4,
    nanoseconds: 892_480_914_569,
  },
  {
    hours: 3954,
    nanoseconds: 571_647_777_618,
  },
  {
    hours: 27,
    nanoseconds: 2322_199_502_640,
  },
  {
    hours: 258054064,
    nanoseconds: 2782_411_891_222,
  },
  {
    hours: 1485,
    nanoseconds: 2422_559_903_100,
  },
  {
    hours: 0,
    nanoseconds: 1461_068_214_153,
  },
  {
    hours: 393,
    nanoseconds: 1250_229_561_658,
  },
  {
    hours: 0,
    nanoseconds: 91_035_820,
  },
  {
    hours: 0,
    nanoseconds: 790_982_655,
  },
  {
    hours: 150,
    nanoseconds: 608_531_524,
  },
  {
    hours: 5469,
    nanoseconds: 889_204_952,
  },
  {
    hours: 7870,
    nanoseconds: 680_042_770,
  },
];

const nsPerHour = 3600_000_000_000;

const fractionDigits = Math.log10(nsPerHour) + Math.log10(100_000_000_000) - Math.log10(36);
assert.sameValue(fractionDigits, 22);

for (let {hours, nanoseconds} of data) {
  assert(nanoseconds < nsPerHour);

  // Compute enough fractional digits to approximate the exact result. Use BigInts
  // to avoid floating point precision loss. Fill to the left with implicit zeros.
  let fraction = ((BigInt(nanoseconds) * 100_000_000_000n) / 36n).toString().padStart(fractionDigits, "0");

  // Get the Number approximation from the string representation.
  let expected = Number(`${hours}.${fraction}`);

  let todayInstant = 0n;
  let tomorrowInstant = BigInt(hours) * BigInt(nsPerHour) + BigInt(nanoseconds);

  let timeZone = new class extends Temporal.TimeZone {
    #getPossibleInstantsFor = 0;

    getPossibleInstantsFor() {
      if (++this.#getPossibleInstantsFor === 1) {
        return [new Temporal.Instant(todayInstant)];
      }
      assert.sameValue(this.#getPossibleInstantsFor, 2);
      return [new Temporal.Instant(tomorrowInstant)];
    }
  }("UTC");

  let zdt = new Temporal.ZonedDateTime(0n, timeZone);
  let actual = zdt.hoursInDay;

  assert.sameValue(
    actual,
    expected,
    `hours=${hours}, nanoseconds=${nanoseconds}`,
  );
}
