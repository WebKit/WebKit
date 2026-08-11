//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}
function shouldThrowRangeError(fn) {
    try { fn(); throw new Error("expected RangeError but no exception thrown"); }
    catch (e) { if (!(e instanceof RangeError)) throw new Error(`expected RangeError, got ${e}`); }
}

const minEpoch = -(86400n * 100_000_000n * 1_000_000_000n);
const maxEpoch =   86400n * 100_000_000n * 1_000_000_000n;

// minEpoch in -12:34 → local date -271821-04-19 (day -100000001, outside ±10^8)
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '-12:34');
    shouldBe(zdt.toString(), '-271821-04-19T11:26:00-12:34[-12:34]');

    // All rounding modes with 1ns increment must return the same value (spec step 13 early return)
    for (const mode of ['ceil','floor','trunc','expand','halfCeil','halfFloor','halfTrunc','halfExpand','halfEven']) {
        const r = zdt.round({ smallestUnit: 'nanosecond', roundingIncrement: 1, roundingMode: mode });
        shouldBe(r.epochNanoseconds, minEpoch);
    }
}

// minEpoch+1ns in -12:34 → local ns=1, 1ns still returns same
{
    const zdt = new Temporal.ZonedDateTime(minEpoch + 1n, '-12:34');
    const r = zdt.round({ smallestUnit: 'nanosecond', roundingMode: 'ceil' });
    shouldBe(r.epochNanoseconds, minEpoch + 1n);
}

// minEpoch in -12:34: local date -271821-04-19 (day -100000001 > 10^8) must throw for non-1ns
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '-12:34');
    for (const unit of ['microsecond', 'millisecond', 'second', 'minute', 'hour']) {
        shouldThrowRangeError(() => zdt.round({ smallestUnit: unit, roundingMode: 'ceil' }));
        shouldThrowRangeError(() => zdt.round({ smallestUnit: unit, roundingMode: 'floor' }));
    }
    // increment > 1 for nanosecond also throws (not covered by spec step 13)
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'nanosecond', roundingIncrement: 2, roundingMode: 'ceil' }));
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'nanosecond', roundingIncrement: 5, roundingMode: 'floor' }));
}

// minEpoch in +01:23 → local date -271821-04-20 (day -100000000, at boundary, valid)
// Rounding up succeeds (result UTC = +00:37, valid); rounding down throws (result UTC < minEpoch)
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '+01:23');
    shouldBe(zdt.toString(), '-271821-04-20T01:23:00+01:23[+01:23]');

    // ceil: 01:23 → 02:00 local → UTC 00:37 (after minEpoch) → valid
    const r1 = zdt.round({ smallestUnit: 'hour', roundingMode: 'ceil' });
    shouldBe(r1.toString(), '-271821-04-20T02:00:00+01:23[+01:23]');

    const r2 = zdt.round({ smallestUnit: 'hour', roundingMode: 'expand' });
    shouldBe(r2.toString(), '-271821-04-20T02:00:00+01:23[+01:23]');

    // floor/trunc: 01:23 → 01:00 local → UTC -271821-04-19T23:37 (before minEpoch) → RangeError
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'hour', roundingMode: 'floor' }));
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'hour', roundingMode: 'trunc' }));

    // half* rounding: 01:23 is closer to 01:00 than 02:00 → rounds down → RangeError
    for (const mode of ['halfCeil','halfFloor','halfTrunc','halfExpand','halfEven']) {
        shouldThrowRangeError(() => zdt.round({ smallestUnit: 'hour', roundingMode: mode }));
    }

    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'day', roundingMode: 'ceil' }));
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'day', roundingMode: 'floor' }));
}

// 1ns at +01:23 boundary also works (spec step 13)
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '+01:23');
    const r = zdt.round({ smallestUnit: 'nanosecond', roundingMode: 'ceil' });
    shouldBe(r.epochNanoseconds, minEpoch);
}

{
    const zdt = Temporal.ZonedDateTime.from('2024-03-15T14:37:22.456789012+05:30[Asia/Kolkata]');
    // 1 hour ceil: 14:37 → 15:00
    const r = zdt.round({ smallestUnit: 'hour', roundingMode: 'ceil' });
    shouldBe(r.hour, 15);
    shouldBe(r.minute, 0);
    // 30 min floor: 14:37 → 14:30
    const r2 = zdt.round({ smallestUnit: 'minute', roundingIncrement: 30, roundingMode: 'floor' });
    shouldBe(r2.hour, 14);
    shouldBe(r2.minute, 30);
}

// maxEpoch UTC = +275760-09-13T00:00:00Z. Any offset ≤ +14:00 gives local date +275760-09-13
// (day +100000000, at boundary, valid). No out-of-range local date is achievable at max boundary.
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '+12:34');
    shouldBe(zdt.toString(), '+275760-09-13T12:34:00+12:34[+12:34]');
    // 1ns step 13 always returns original
    const r = zdt.round({ smallestUnit: 'nanosecond', roundingMode: 'floor' });
    shouldBe(r.epochNanoseconds, maxEpoch);
    // hour trunc: 12:34 → 12:00 local → UTC -0:34 from day end → valid
    const r2 = zdt.round({ smallestUnit: 'hour', roundingMode: 'trunc' });
    shouldBe(r2.toString(), '+275760-09-13T12:00:00+12:34[+12:34]');
    // hour ceil: 12:34 → 13:00 local → UTC = maxEpoch + 26min → exceeds max epoch → RangeError
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'hour', roundingMode: 'ceil' }));
}

// maxEpoch in UTC: day rounding calls getStartOfDay(date) and getStartOfDay(nextDate=+275760-09-14).
// +275760-09-14 is one day past the spec limit → getStartOfDay should propagate error → RangeError.
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '+00:00');
    shouldBe(zdt.toString(), '+275760-09-13T00:00:00+00:00[+00:00]');
    // Any day rounding throws because getStartOfDay(nextDate) goes out of range.
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'day', roundingMode: 'ceil' }));
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'day', roundingMode: 'floor' }));
    shouldThrowRangeError(() => zdt.round({ smallestUnit: 'day', roundingMode: 'trunc' }));
}

// toZonedDateTime() calls getStartOfDay. At max epoch UTC, the start of +275760-09-13 is exactly
// maxEpoch (since UTC midnight = maxEpoch). Valid result.
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '+00:00');
    const sd = zdt.toPlainDate().toZonedDateTime({ timeZone: '+00:00' });
    shouldBe(sd.epochNanoseconds, maxEpoch); // midnight = maxEpoch
    shouldBe(sd.hour, 0);
    shouldBe(sd.minute, 0);
}
// maxEpoch in -12:34: local date +275760-09-12 (valid), startOfDay = +275760-09-12T00:00:00-12:34
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '-12:34');
    const sd = zdt.toPlainDate().toZonedDateTime({ timeZone: '-12:34' });
    shouldBe(sd.hour, 0);
    shouldBe(sd.minute, 0);
    shouldBe(sd.year, 275760);
    shouldBe(sd.month, 9);
    shouldBe(sd.day, 12);
}
