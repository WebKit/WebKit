//@ requireOptions("--useTemporal=1")

// Pacific/Apia 2011-12-30 date-line skip: at 2011-12-30T10:00:00Z the offset jumped
// from -10:00 to +14:00. All wall times 2011-12-30T00:00 .. 23:59:59 are nonexistent.
// pre-gap offset (era right before): -10:00
// post-gap offset:                    +14:00

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}
function shouldThrowRangeError(fn) {
    try { fn(); throw new Error("expected RangeError but no exception thrown"); }
    catch (e) { if (!(e instanceof RangeError)) throw new Error(`expected RangeError, got ${e}`); }
}

const tz = 'Pacific/Apia';
// At/around the transition instant.
const transitionEpochNs = 1325239200000000000n; // 2011-12-30T10:00:00Z

// 1. PlainDateTime in the gap, all four disambiguations.
{
    const mid = Temporal.PlainDateTime.from('2011-12-30T00:00:00');

    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    shouldBe(e.offset, '-10:00');
    shouldBe(e.epochNanoseconds, transitionEpochNs - 86400000000000n); // 24h before
    // earlier epoch = naive(Dec 30 00:00 UTC) - afterOffset(+14h) = Dec 29 10:00 UTC.
    // Local with offset -10:00 = Dec 29 00:00.
    shouldBe(e.year, 2011); shouldBe(e.month, 12); shouldBe(e.day, 29);
    shouldBe(e.hour, 0); shouldBe(e.minute, 0); shouldBe(e.second, 0);

    // 'later' / 'compatible': bracketed by pre-gap offset (-10:00).
    // naive = 2011-12-30T00:00 (as UTC) → epoch = naive + 10h = transition instant.
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    shouldBe(l.offset, '+14:00');
    shouldBe(l.epochNanoseconds, transitionEpochNs);

    const c = mid.toZonedDateTime(tz, { disambiguation: 'compatible' });
    shouldBe(c.epochNanoseconds, l.epochNanoseconds);

    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));

    // Mid-gap: 2011-12-30T12:00 → 'later' = naive + 10h = 2011-12-30T22:00Z
    //                              'earlier' = naive - 14h = 2011-12-29T22:00Z
    const noon = Temporal.PlainDateTime.from('2011-12-30T12:00:00');
    const noonE = noon.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const noonL = noon.toZonedDateTime(tz, { disambiguation: 'later' });
    shouldBe(noonE.epochNanoseconds, 1325196000000000000n); // 2011-12-29T22:00Z, -10:00
    shouldBe(noonL.epochNanoseconds, 1325282400000000000n); // 2011-12-30T22:00Z, +14:00
    shouldBe(noonL.epochNanoseconds - noonE.epochNanoseconds, 86400000000000n); // 24h apart
}

// 2. ZonedDateTime calendar arithmetic: subtract P1D over the date-line skip.
//    'compatible' is the disambiguation used internally by AddZonedDateTime.
{
    // Day after the skip: 2011-12-31T00:00 +14 = transition instant.
    const start = Temporal.ZonedDateTime.from('2011-12-31T00:00[Pacific/Apia]');
    shouldBe(start.epochNanoseconds, transitionEpochNs);

    // Subtracting 1 calendar day lands on 2011-12-30T00:00 wall (gap) → 'compatible' = transition.
    const minusOne = start.subtract({ days: 1 });
    shouldBe(minusOne.epochNanoseconds, transitionEpochNs);
    shouldBe(minusOne.offset, '+14:00');

    // Adding back 1 day lands on 2012-01-01T00:00.
    const back = minusOne.add({ days: 1 });
    shouldBe(back.year, 2012); shouldBe(back.month, 1); shouldBe(back.day, 1);
    shouldBe(back.hour, 0); shouldBe(back.minute, 0);
    shouldBe(back.offset, '+14:00');
}

// 3. ZonedDateTime.from with the bare gap wall time + each disambiguation.
{
    const fromE = Temporal.ZonedDateTime.from(
        { year: 2011, month: 12, day: 30, hour: 0, minute: 0, timeZone: tz },
        { disambiguation: 'earlier' });
    shouldBe(fromE.offset, '-10:00');
    shouldBe(fromE.epochNanoseconds, transitionEpochNs - 86400000000000n);

    const fromL = Temporal.ZonedDateTime.from(
        { year: 2011, month: 12, day: 30, hour: 0, minute: 0, timeZone: tz },
        { disambiguation: 'later' });
    shouldBe(fromL.offset, '+14:00');
    shouldBe(fromL.epochNanoseconds, transitionEpochNs);

    shouldThrowRangeError(() => Temporal.ZonedDateTime.from(
        { year: 2011, month: 12, day: 30, hour: 12, minute: 0, timeZone: tz },
        { disambiguation: 'reject' }));
}
