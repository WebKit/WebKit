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

// minEpoch in -12:34: local date -271821-04-19 (day -100000001, outside ±10^8)
// Any with() that results in this date must throw.
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '-12:34');
    shouldBe(zdt.toString(), '-271821-04-19T11:26:00-12:34[-12:34]');

    // Modifying sub-second fields keeps same date → RangeError (from benchmark case)
    shouldThrowRangeError(() => zdt.with({ year: -271821, microsecond: 1, nanosecond: 0 }));
    shouldThrowRangeError(() => zdt.with({ year: -271821, microsecond: 1, nanosecond: 0 }, { overflow: 'constrain' }));
    shouldThrowRangeError(() => zdt.with({ microsecond: 1 }));
    shouldThrowRangeError(() => zdt.with({ nanosecond: 999 }));
    shouldThrowRangeError(() => zdt.with({ hour: 12 }));
    shouldThrowRangeError(() => zdt.with({ minute: 0 }));

    // Default offset option is 'prefer' — also throws
    shouldThrowRangeError(() => zdt.with({ second: 30 }, { offset: 'prefer' }));
    shouldThrowRangeError(() => zdt.with({ second: 30 }, { offset: 'reject' }));

    // 'use' branch computes balanced UTC date — also throws because balanced UTC date
    // -271821-04-20 would give valid epoch, but the local date check still runs
}

// minEpoch in +01:23: local date -271821-04-20 (day -100000000, at boundary, valid)
// with() that keeps same local date must succeed.
{
    const zdt = new Temporal.ZonedDateTime(minEpoch, '+01:23');
    shouldBe(zdt.toString(), '-271821-04-20T01:23:00+01:23[+01:23]');

    // Modifying time fields within same day succeeds
    const r1 = zdt.with({ hour: 2, minute: 0 });
    shouldBe(r1.toString(), '-271821-04-20T02:00:00+01:23[+01:23]');

    const r2 = zdt.with({ nanosecond: 500 });
    shouldBe(r2.toString(), '-271821-04-20T01:23:00.0000005+01:23[+01:23]');

    // with({day:19}) pushes local date to -271821-04-19 (day -100000001, out of range) → RangeError
    // This is distinct from the above tests: the *starting* local date is valid but the *result* is not.
    shouldThrowRangeError(() => zdt.with({ day: 19 }));
    shouldThrowRangeError(() => zdt.with({ day: 19 }, { overflow: 'constrain' }));
}

// ZDT one day ahead of minEpoch in -12:34: local date -271821-04-20 (day -100000000, valid)
// with() must succeed.
{
    const oneDayNs = 86400n * 1_000_000_000n;
    const zdt = new Temporal.ZonedDateTime(minEpoch + oneDayNs, '-12:34');
    shouldBe(zdt.toString(), '-271821-04-20T11:26:00-12:34[-12:34]');

    // Changing hour only (minute stays 26)
    const r = zdt.with({ hour: 15 });
    shouldBe(r.toString(), '-271821-04-20T15:26:00-12:34[-12:34]');
    // Changing hour and minute
    const r2 = zdt.with({ hour: 15, minute: 0 });
    shouldBe(r2.toString(), '-271821-04-20T15:00:00-12:34[-12:34]');
}

// maxEpoch boundary: maxEpoch UTC = +275760-09-13T00:00:00Z.
// Any offset ≤ +14:00 gives local date +275760-09-13 (day +100000000, valid).
// No out-of-range local date is achievable at max boundary (would need UTC > maxEpoch).
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '+12:34');
    shouldBe(zdt.toString(), '+275760-09-13T12:34:00+12:34[+12:34]');
    // with() succeeds for valid local date
    const r = zdt.with({ minute: 0 });
    shouldBe(r.toString(), '+275760-09-13T12:00:00+12:34[+12:34]');
}

// maxEpoch in -01:23: local date +275760-09-12 (safely within range)
{
    const zdt = new Temporal.ZonedDateTime(maxEpoch, '-01:23');
    shouldBe(zdt.toString(), '+275760-09-12T22:37:00-01:23[-01:23]');
    // Modifying within same day is fine
    const r = zdt.with({ hour: 20 });
    shouldBe(r.toString(), '+275760-09-12T20:37:00-01:23[-01:23]');
}

{
    const zdt = Temporal.ZonedDateTime.from('2024-06-15T10:30:00+09:00[Asia/Tokyo]');
    const r = zdt.with({ hour: 14, minute: 45, second: 30 });
    shouldBe(r.hour, 14);
    shouldBe(r.minute, 45);
    shouldBe(r.second, 30);
    shouldBe(r.timeZoneId, 'Asia/Tokyo');
}
