//@ requireOptions("--useTemporal=1")

// Rounding a Duration relative to a ZonedDateTime whose calendar day is entirely
// skipped by a time zone transition (e.g. Pacific/Apia skipped 2011-12-30, jumping
// from -10:00 to +14:00) produces a zero-length nudge window in NudgeToCalendarUnit:
// startEpochNs equals endEpochNs, so computing the progress fraction divides by zero.
// This must throw a RangeError instead of dividing by zero.
//
// NudgeToZonedTime reaches the same zero-length day from its step 3, which computes
// endDate = AddDaysToISODate(start, sign): with sign = -1 that steps backward onto the
// skipped day, so steps 5-6 resolve both bounds to one instant and daySpan is 0. There
// the span is only ever subtracted, never divided by, so it computes normally -- only
// step 8's assertion had to stop claiming TimeDurationSign(daySpan) is never 0.
// https://github.com/tc39/proposal-temporal/issues/3310

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}
function shouldThrowRangeError(fn) {
    try { fn(); } catch (e) {
        if (!(e instanceof RangeError))
            throw new Error(`expected RangeError, got ${e}`);
        return;
    }
    throw new Error("expected RangeError but no exception thrown");
}

// Pacific/Apia: 2011-12-30 does not exist. The one-day window ending on the skipped
// day collapses: both bounds resolve to the same instant.
{
    const relativeTo = Temporal.ZonedDateTime.from('2012-01-01T12:00:00[Pacific/Apia]');
    const oneDayAgo = Temporal.Duration.from({ days: -1 });
    shouldThrowRangeError(() => oneDayAgo.round({ smallestUnit: 'days', relativeTo }));
    shouldThrowRangeError(() => oneDayAgo.total({ unit: 'days', relativeTo }));
    shouldThrowRangeError(() => relativeTo.until(relativeTo.add(oneDayAgo), { smallestUnit: 'days' }));
}

// Same, starting inside the day right after the gap with a sub-day remainder.
{
    const relativeTo = Temporal.ZonedDateTime.from('2011-12-31T00:30:00+14:00[Pacific/Apia]');
    shouldThrowRangeError(() => Temporal.Duration.from('-PT30M').total({ unit: 'days', relativeTo }));
}

// Here the retry window (additionalShift=true) still does not contain the destination.
{
    const relativeTo = Temporal.ZonedDateTime.from('2011-12-31T12:00:00+14:00[Pacific/Apia]');
    const duration = Temporal.Duration.from({ days: -1, hours: -12 });
    shouldThrowRangeError(() => duration.round({ smallestUnit: 'day', relativeTo }));
    shouldThrowRangeError(() => duration.total({ unit: 'day', relativeTo }));
}

// Pacific/Kiritimati: 1994-12-31 does not exist.
{
    const relativeTo = Temporal.ZonedDateTime.from('1995-01-02T12:00:00[Pacific/Kiritimati]');
    const oneDayAgo = Temporal.Duration.from({ days: -1 });
    shouldThrowRangeError(() => oneDayAgo.round({ smallestUnit: 'days', relativeTo }));
    shouldThrowRangeError(() => oneDayAgo.total({ unit: 'days', relativeTo }));
    shouldThrowRangeError(() => relativeTo.until(relativeTo.add(oneDayAgo), { smallestUnit: 'days' }));
}

// Ordinary DST transitions (1-hour shifts) can leave the destination outside the
// nudge window even after the additionalShift retry, but the window is not
// zero-length; this must keep computing the same result as before.
{
    const x = Temporal.ZonedDateTime.from('1997-09-21T23:00:02.000000002+03:30[Asia/Tehran]');
    const y = Temporal.ZonedDateTime.from('1996-07-17T23:59:07.540000748+04:30[Asia/Tehran]');
    shouldBe(y.since(x, { smallestUnit: 'day', roundingMode: 'floor', largestUnit: 'year' }).toString(), '-P1Y2M4D');
}

// NudgeToZonedTime: a TIME smallestUnit with a zoned relativeTo takes step 6 instead of
// the irregular-length branch above, so daySpan = 0 reaches step 8's assertion rather
// than the division. These must produce answers. Values agree with V8 and SpiderMonkey.
{
    const cases = [
        ['Pacific/Apia', '2011-12-31T12:00', { days: -1, minutes: -30 }, 'hour', '-P1D'],
        ['Pacific/Apia', '2011-12-31T12:00', { days: -1, minutes: -30 }, 'minute', '-P1DT30M'],
        ['Pacific/Apia', '2011-12-31T12:00', { days: -1, seconds: -1 }, 'hour', '-P1D'],
        ['Pacific/Apia', '2011-12-31T00:00', { days: -1, minutes: -30 }, 'minute', '-P1DT30M'],
        ['Pacific/Kiritimati', '1995-01-01T12:00', { days: -1, minutes: -30 }, 'hour', '-P1D'],
        ['Pacific/Kiritimati', '1995-01-01T12:00', { days: -1, minutes: -30 }, 'minute', '-P1DT30M'],
        ['Pacific/Kiritimati', '1995-01-01T06:30', { days: -1, seconds: -1 }, 'minute', '-P1D'],
    ];
    for (const [timeZone, iso, duration, smallestUnit, expected] of cases) {
        const relativeTo = Temporal.PlainDateTime.from(iso).toZonedDateTime(timeZone);
        shouldBe(Temporal.Duration.from(duration).round({
            largestUnit: 'day', smallestUnit, roundingMode: 'ceil', relativeTo }).toString(), expected);
    }
}

// Guard against tzdata dropping either gap, which would make every case above vacuous:
// the day preceding each reference date must have zero length.
{
    const precedingDayHours = (timeZone, date) => {
        const at = d => Temporal.PlainDateTime.from(`${d}T12:00`).toZonedDateTime(timeZone).epochNanoseconds;
        return Number((at(date) - at(Temporal.PlainDate.from(date).subtract({ days: 1 }).toString())) / 3600000000000n);
    };
    shouldBe(precedingDayHours('Pacific/Apia', '2011-12-31'), 0);
    shouldBe(precedingDayHours('Pacific/Kiritimati', '1995-01-01'), 0);
}

