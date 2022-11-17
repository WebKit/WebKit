// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.hoursinday
description: >
  Hours in day is correctly rounded using precise mathematical values.
info: |
  get Temporal.ZonedDateTime.prototype.hoursInDay

  ...
  15. Let diffNs be tomorrowInstant.[[Nanoseconds]] - todayInstant.[[Nanoseconds]].
  16. Return 𝔽(diffNs / (3.6 × 10^12)).
features: [Temporal]
---*/

const maxInstant = 86_40000_00000_00000_00000n;

function nextUp(num) {
  if (!Number.isFinite(num)) {
    return num;
  }
  if (num === 0) {
    return Number.MIN_VALUE;
  }

  var f64 = new Float64Array([num]);
  var u64 = new BigUint64Array(f64.buffer);
  u64[0] += (num < 0 ? -1n : 1n);
  return f64[0];
}

function nextDown(num) {
  if (!Number.isFinite(num)) {
    return num;
  }
  if (num === 0) {
    return -Number.MIN_VALUE;
  }

  var f64 = new Float64Array([num]);
  var u64 = new BigUint64Array(f64.buffer);
  u64[0] += (num < 0 ? 1n : -1n);
  return f64[0];
}

function BigIntAbs(n) {
  return n >= 0 ? n : -n;
}

function test(todayInstant, tomorrowInstant, expected) {
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
  let zdt_hoursInDay = zdt.hoursInDay;

  assert.sameValue(zdt_hoursInDay, expected, "hoursInDay return value");

  // Ensure the |expected| value is actually correctly rounded.

  const nsPerSec = 1000n * 1000n * 1000n;
  const secPerHour = 60n * 60n;
  const nsPerHour = secPerHour * nsPerSec;

  function toNanoseconds(hours) {
    let wholeHours = BigInt(Math.trunc(hours)) * nsPerHour;
    let fractionalHours = BigInt(Math.trunc(hours % 1 * Number(nsPerHour)));
    return wholeHours + fractionalHours;
  }

  let diff = tomorrowInstant - todayInstant;
  let nanosInDay = toNanoseconds(zdt_hoursInDay);

  // The next number gives a less precise result.
  let next = toNanoseconds(nextUp(zdt_hoursInDay));
  assert(BigIntAbs(diff - nanosInDay) <= BigIntAbs(diff - next));

  // The previous number gives a less precise result.
  let prev = toNanoseconds(nextDown(zdt_hoursInDay));
  assert(BigIntAbs(diff - nanosInDay) <= BigIntAbs(diff - prev));

  // This computation can be inaccurate.
  let inaccurate = Number(diff) / Number(nsPerHour);

  // Doing it component-wise can produce more accurate results.
  let hours = Number(diff / nsPerSec) / Number(secPerHour);
  let fractionalHours = Number(diff % nsPerSec) / Number(nsPerHour);
  assert.sameValue(hours + fractionalHours, expected);

  // Ensure the result is more precise than the inaccurate result.
  let inaccurateNanosInDay = toNanoseconds(inaccurate);
  assert(BigIntAbs(diff - nanosInDay) <= BigIntAbs(diff - inaccurateNanosInDay));
}

test(-maxInstant,   0n, 2400000000);
test(-maxInstant,   1n, 2400000000);
test(-maxInstant,  10n, 2400000000);
test(-maxInstant, 100n, 2400000000);

test(-maxInstant,   1_000n, 2400000000);
test(-maxInstant,  10_000n, 2400000000);
test(-maxInstant, 100_000n, 2400000000);

test(-maxInstant,   1_000_000n, 2400000000.0000005);
test(-maxInstant,  10_000_000n, 2400000000.000003);
test(-maxInstant, 100_000_000n, 2400000000.0000277);

test(-maxInstant,   1_000_000_000n, 2400000000.000278);
test(-maxInstant,  10_000_000_000n, 2400000000.0027776);
test(-maxInstant, 100_000_000_000n, 2400000000.0277777);

test(-maxInstant, maxInstant, 4800000000);
test(-maxInstant, maxInstant - 10_000_000_000n, 4799999999.997222);
test(-maxInstant, maxInstant - 10_000_000_000n + 1n, 4799999999.997222);
test(-maxInstant, maxInstant - 10_000_000_000n - 1n, 4799999999.997222);

test(maxInstant, -maxInstant, -4800000000);
test(maxInstant, -(maxInstant - 10_000_000_000n), -4799999999.997222);
test(maxInstant, -(maxInstant - 10_000_000_000n + 1n), -4799999999.997222);
test(maxInstant, -(maxInstant - 10_000_000_000n - 1n), -4799999999.997222);
