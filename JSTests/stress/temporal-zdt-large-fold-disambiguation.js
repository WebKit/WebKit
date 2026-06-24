//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}
function shouldThrowRangeError(fn) {
    try { fn(); throw new Error("expected RangeError but no exception thrown"); }
    catch (e) { if (!(e instanceof RangeError)) throw new Error(`expected RangeError, got ${e}`); }
}

// Alaska Purchase: Aleutian Islands crossed the date line, -24h fall-back.
// Before: +12:13:22 (Russian side), After: -11:46:38 (US side).
// This was the primary failing case (earlier → wrong offset -11:47).
{
    const transition = '1867-10-19T00:44:35'; // mid-fold: ambiguous local time
    const tz = 'America/Adak';
    const mid = Temporal.PlainDateTime.from(transition);

    // Earlier (pre-fold, Russian side offset) — this was the broken case
    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    shouldBe(e.offset, '+12:13:22');
    shouldBe(e.year, 1867); shouldBe(e.month, 10); shouldBe(e.day, 19);
    shouldBe(e.hour, 0); shouldBe(e.minute, 44); shouldBe(e.second, 35);

    // Later (post-fold, US side offset)
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    shouldBe(l.offset, '-11:46:38');
    shouldBe(l.year, 1867); shouldBe(l.month, 10); shouldBe(l.day, 19);
    shouldBe(l.hour, 0); shouldBe(l.minute, 44); shouldBe(l.second, 35);

    const c = mid.toZonedDateTime(tz, { disambiguation: 'compatible' });
    shouldBe(c.epochNanoseconds, e.epochNanoseconds);

    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));

    const ef = Temporal.ZonedDateTime.from({ year: 1867, monthCode: 'M10', day: 19,
        hour: 0, minute: 44, second: 35, timeZone: tz }, { disambiguation: 'earlier' });
    const lf = Temporal.ZonedDateTime.from({ year: 1867, monthCode: 'M10', day: 19,
        hour: 0, minute: 44, second: 35, timeZone: tz }, { disambiguation: 'later' });
    shouldBe(ef.epochNanoseconds, e.epochNanoseconds);
    shouldBe(lf.epochNanoseconds, l.epochNanoseconds);

    shouldBe(e.with({ offset: l.offset }).epochNanoseconds, l.epochNanoseconds);
    shouldBe(l.with({ offset: e.offset }).epochNanoseconds, e.epochNanoseconds);

    const diffNs = l.epochNanoseconds - e.epochNanoseconds;
    shouldBe(diffNs, 86400000000000n);
}

// Same Alaska Purchase event, different Alaskan city. -24h fall-back.
{
    const tz = 'America/Anchorage';
    const mid = Temporal.PlainDateTime.from('1867-10-19T02:31:37');

    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });

    shouldBe(e.offset, '+14:00:24');
    shouldBe(l.offset, '-09:59:36');
    shouldBe(e.epochNanoseconds < l.epochNanoseconds, true);
    shouldBe(l.epochNanoseconds - e.epochNanoseconds, 86400000000000n); // 24h apart

    const c = mid.toZonedDateTime(tz, { disambiguation: 'compatible' });
    shouldBe(c.epochNanoseconds, e.epochNanoseconds);

    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}

// Samoa crossed the date line from east to west, -24h fall-back.
{
    const tz = 'Pacific/Apia';
    const mid = Temporal.PlainDateTime.from('1892-07-04T12:00:00');

    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });

    shouldBe(e.offset, '+12:33:04');
    shouldBe(l.offset, '-11:26:56');
    shouldBe(l.epochNanoseconds - e.epochNanoseconds, 86400000000000n); // 24h apart

    shouldBe(e.with({ offset: l.offset }).epochNanoseconds, l.epochNanoseconds);
    shouldBe(l.with({ offset: e.offset }).epochNanoseconds, e.epochNanoseconds);
    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}

// Kwajalein switched sides, ~23h fall-back (not quite 24 due to non-integer offset).
{
    const tz = 'Pacific/Kwajalein';
    const mid = Temporal.PlainDateTime.from('1969-09-30T12:30:00');

    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });

    shouldBe(e.offset, '+11:00');
    shouldBe(l.offset, '-12:00');
    shouldBe(e.epochNanoseconds < l.epochNanoseconds, true);

    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}

// Expanding the fold window must not break standard 1h DST folds.
{
    // 2024-11-03 01:30 America/Los_Angeles — fall back from PDT to PST
    const mid = Temporal.PlainDateTime.from('2024-11-03T01:30:00');
    const tz = 'America/Los_Angeles';

    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    const c = mid.toZonedDateTime(tz, { disambiguation: 'compatible' });

    shouldBe(e.offset, '-07:00');  // PDT (summer time, earlier UTC)
    shouldBe(l.offset, '-08:00');  // PST (winter time, later UTC)
    shouldBe(c.epochNanoseconds, e.epochNanoseconds); // compatible = earlier for fall-back
    shouldBe(l.epochNanoseconds - e.epochNanoseconds, 3600000000000n); // 1h apart

    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));

    // with({offset}) switching
    shouldBe(e.with({ offset: '-08:00' }).epochNanoseconds, l.epochNanoseconds);
    shouldBe(l.with({ offset: '-07:00' }).epochNanoseconds, e.epochNanoseconds);
}

{
    const td = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    const t = Temporal.Instant.from('1850-01-01T00Z')
        .toZonedDateTimeISO('America/Adak')
        .getTimeZoneTransition('next');

    // The transition ZDT itself carries the post-transition (new) offset
    const after = t.add(td);
    shouldBe(t.offsetNanoseconds, after.offsetNanoseconds);

    // Wall-clock distance between 1ns before and 1ns after = |offsetChange| + 2ns
    const before = t.subtract(td);
    const offsetChange = after.offsetNanoseconds - before.offsetNanoseconds; // -24h in ns
    const wallBefore = before.toPlainDateTime();
    const wallAfter  = after.toPlainDateTime();
    const wallDiffNs = BigInt(wallBefore.until(wallAfter, { largestUnit: 'nanoseconds' }).nanoseconds);
    // offsetChange is negative (fall-back), so wall diff = offsetChange + 2ns
    shouldBe(wallDiffNs, BigInt(offsetChange) + 2n);
}

// Same date-line crossing pattern as Alaska but for Samoa.
{
    const tz = 'Pacific/Apia';
    const mid = Temporal.PlainDateTime.from('1892-07-04T12:00:00');
    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    shouldBe(e.offset, '+12:33:04');
    shouldBe(l.offset, '-11:26:56');
    shouldBe(l.epochNanoseconds - e.epochNanoseconds, 86400000000000n);
    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
    shouldBe(mid.toZonedDateTime(tz, { disambiguation: 'compatible' }).epochNanoseconds, e.epochNanoseconds);
}

// ─── Pacific/Midway 1892 (24h fall-back, Midway Islands date-line crossing) ─
{
    const tz = 'Pacific/Midway';
    const mid = Temporal.PlainDateTime.from('1892-07-04T12:00:00');
    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    shouldBe(e.offset, '+12:37:12');
    shouldBe(l.offset, '-11:22:48');
    shouldBe(l.epochNanoseconds - e.epochNanoseconds, 86400000000000n);
    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}

// Tests the expanded fold window also handles large spring-forwards (gaps).
{
    const tz = 'Antarctica/Davis';
    // 1957-01-13T03:30 is in the 7h DST gap (spring forward from UTC+0 to UTC+7)
    const mid = Temporal.PlainDateTime.from('1957-01-13T03:30:00');
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    shouldBe(l.offset, '+07:00');
    shouldBe(e.offset, '+00:00');
    // compatible = later for spring-forward (gap)
    shouldBe(mid.toZonedDateTime(tz, { disambiguation: 'compatible' }).epochNanoseconds, l.epochNanoseconds);
    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}

{
    const tz = 'Antarctica/Casey';
    const mid = Temporal.PlainDateTime.from('1969-01-01T04:00:00');
    const l = mid.toZonedDateTime(tz, { disambiguation: 'later' });
    const e = mid.toZonedDateTime(tz, { disambiguation: 'earlier' });
    shouldBe(l.offset, '+08:00');
    shouldBe(e.offset, '+00:00');
    shouldBe(mid.toZonedDateTime(tz, { disambiguation: 'compatible' }).epochNanoseconds, l.epochNanoseconds);
    shouldThrowRangeError(() => mid.toZonedDateTime(tz, { disambiguation: 'reject' }));
}
