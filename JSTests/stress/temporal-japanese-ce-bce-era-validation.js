//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try { func(); } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected ${errorType.name} but no exception was thrown`);
}

// Japanese "ce"/"bce" era fast path: out-of-range month must be constrained
// (not read out of bounds and produce dates like "2000-255-00").
{
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 255, day: 1 });
    shouldBe(r.toString(), "2000-12-01[u-ca=japanese]");
    shouldBe(r.month, 12);
    shouldBe(r.day, 1);
}
{
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 13, day: 1 });
    shouldBe(r.toString(), "2000-12-01[u-ca=japanese]");
}
{
    // ISO year 0 (bce 1); assert via toString since the year getter goes
    // through ICU and is subject to Julian/Gregorian switch quirks near year 0.
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "bce", eraYear: 1, month: 255, day: 40 });
    shouldBe(r.toString(), "0000-12-31[u-ca=japanese]");
    shouldBe(r.month, 12);
    shouldBe(r.day, 31);
}
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 255, day: 1 }, { overflow: "reject" });
}, RangeError);
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 13, day: 1 }, { overflow: "reject" });
}, RangeError);

// PlainYearMonth.from through the same fast path.
{
    const r = Temporal.PlainYearMonth.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 255 });
    shouldBe(r.year, 2000);
    shouldBe(r.month, 12);
}
shouldThrow(() => {
    Temporal.PlainYearMonth.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 255 }, { overflow: "reject" });
}, RangeError);

// ZonedDateTime.with / PlainDateTime.with reach the fast path without the
// fields-layer rough year range check, so the fast path itself must validate.
{
    const z = Temporal.ZonedDateTime.from("2000-01-01T00:00[UTC][u-ca=japanese]");
    shouldBe(z.with({ era: "ce", eraYear: 2000, month: 255 }).toString(), "2000-12-01T00:00:00+00:00[UTC][u-ca=japanese]");
    // eraYear INT32_MAX silently truncated the 21-bit ISO year field (year became -1).
    shouldThrow(() => { z.with({ era: "ce", eraYear: 2147483647 }); }, RangeError);
    // 1 - INT32_MIN overflows int32.
    shouldThrow(() => { z.with({ era: "bce", eraYear: -2147483648 }); }, RangeError);
    shouldThrow(() => { z.with({ era: "ce", eraYear: 300000 }); }, RangeError);
    shouldThrow(() => { z.with({ era: "bce", eraYear: 300000 }); }, RangeError);
}
{
    const pdt = Temporal.PlainDateTime.from("2000-01-01T00:00[u-ca=japanese]");
    shouldBe(pdt.with({ era: "ce", eraYear: 2000, month: 255 }).toString(), "2000-12-01T00:00:00[u-ca=japanese]");
    shouldThrow(() => { pdt.with({ era: "ce", eraYear: 2147483647 }); }, RangeError);
    shouldThrow(() => { pdt.with({ era: "bce", eraYear: -2147483648 }); }, RangeError);
}

// eraYear out of ISO year limits must throw RangeError (Debug builds asserted
// in the ISO8601::PlainDate constructor before the fix).
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 300000, month: 1, day: 1 });
}, RangeError);
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "bce", eraYear: 300000, month: 1, day: 1 });
}, RangeError);
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 275761, month: 1, day: 1 });
}, RangeError);
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "bce", eraYear: 271823, month: 1, day: 1 });
}, RangeError);

// Boundary years remain accepted.
{
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 275760, month: 9, day: 13 });
    shouldBe(r.year, 275760);
    shouldBe(r.month, 9);
    shouldBe(r.day, 13);
}
{
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "bce", eraYear: 271822, month: 4, day: 19 });
    shouldBe(r.toString(), "-271821-04-19[u-ca=japanese]");
    shouldBe(r.month, 4);
    shouldBe(r.day, 19);
}

// Normal ce/bce dates still resolve correctly.
{
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, month: 2, day: 29 });
    shouldBe(r.toString(), "2000-02-29[u-ca=japanese]");
    // The era getters report the canonical era for the date, not the input.
    shouldBe(r.era, "heisei");
    shouldBe(r.eraYear, 12);
}
{
    // bce 1 = ISO year 0, which is a leap year.
    const r = Temporal.PlainDate.from({ calendar: "japanese", era: "bce", eraYear: 1, month: 2, day: 29 });
    shouldBe(r.year, 0);
    shouldBe(r.month, 2);
    shouldBe(r.day, 29);
}

// year-consistency check with era+eraYear still applies.
shouldThrow(() => {
    Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 2000, year: 1999, month: 1, day: 1 });
}, RangeError);
