//@ requireOptions("--useTemporal=1")

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

const overlongDays = [31, 32, 255, 256, 257, 258, 512, 513, 2147483647, 2147483648, 1e10, Number.MAX_SAFE_INTEGER];

// ISO: every value at or past the length of the month clamps to the last day.
{
    const jan = Temporal.PlainYearMonth.from('2024-01');
    for (const day of overlongDays)
        shouldBe(jan.toPlainDate({ day }).toString(), '2024-01-31');
    shouldBe(jan.toPlainDate({ day: 1 }).toString(), '2024-01-01');
    shouldBe(jan.toPlainDate({ day: 30 }).toString(), '2024-01-30');

    // February of a leap year clamps to 29, not 31.
    shouldBe(Temporal.PlainYearMonth.from('2024-02').toPlainDate({ day: 300 }).toString(), '2024-02-29');
    shouldBe(Temporal.PlainYearMonth.from('2023-02').toPlainDate({ day: 300 }).toString(), '2023-02-28');
}

// Non-ISO calendars clamp the same way, including the Gregorian-structured ones that bypass ICU.
{
    const gregory = Temporal.PlainYearMonth.from({ year: 2024, month: 1, calendar: 'gregory' });
    for (const day of overlongDays)
        shouldBe(gregory.toPlainDate({ day }).toString(), '2024-01-31[u-ca=gregory]');
    shouldBe(gregory.toPlainDate({ day: 1 }).toString(), '2024-01-01[u-ca=gregory]');

    const buddhist = Temporal.PlainYearMonth.from({ year: 2567, month: 1, calendar: 'buddhist' });
    for (const day of overlongDays) {
        const d = buddhist.toPlainDate({ day });
        shouldBe(d.toString(), '2024-01-31[u-ca=buddhist]');
        // The result must be a representable date, so its own serialization round-trips.
        shouldBe(Temporal.PlainDate.from(d.toString()).toString(), '2024-01-31[u-ca=buddhist]');
        shouldBe(d.day, 31);
    }

    // A lunisolar calendar whose months are at most 30 days.
    const hebrew = Temporal.PlainYearMonth.from({ year: 5784, monthCode: 'M01', calendar: 'hebrew' });
    for (const day of overlongDays)
        shouldBe(hebrew.toPlainDate({ day }).day, 30);
}

// day must still be rejected outright when it is not a positive integer.
{
    const jan = Temporal.PlainYearMonth.from('2024-01');
    shouldThrowRangeError(() => jan.toPlainDate({ day: 0 }));
    shouldThrowRangeError(() => jan.toPlainDate({ day: -1 }));
    shouldThrowRangeError(() => jan.toPlainDate({ day: Infinity }));
    shouldThrowRangeError(() => jan.toPlainDate({ day: -Infinity }));
    shouldThrowRangeError(() => jan.toPlainDate({ day: NaN }));
}

// An overlong day under overflow 'reject' is a RangeError rather than a clamp. toPlainDate itself
// pins ~constrain~, so exercise the same resolve path through from().
{
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 2024, month: 1, day: 257 }, { overflow: 'reject' }));
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 2024, month: 1, day: 256, calendar: 'gregory' }, { overflow: 'reject' }));
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 2567, month: 1, day: 256, calendar: 'buddhist' }, { overflow: 'reject' }));
    shouldBe(Temporal.PlainDate.from({ year: 2024, month: 1, day: 257 }).toString(), '2024-01-31');
}
