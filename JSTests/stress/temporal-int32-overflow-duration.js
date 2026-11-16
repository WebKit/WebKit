//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Bug 1: calendarDateAdd (ICU bridge) cast int64_t duration fields to int32_t before
// passing to ucal_add. 4294967295 = UINT32_MAX truncates to int32_t(-1), so
// 2020 + (-1) = 2019 instead of RangeError.

// PlainYearMonth.add — large positive years
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ years: 4294967295 }), RangeError);
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ years: 2147483648 }), RangeError);  // INT32_MAX + 1
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ years: 1e15 }), RangeError);

// PlainYearMonth.add — large negative years
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ years: -4294967295 }), RangeError);
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ years: -2147483649 }), RangeError); // INT32_MIN - 1

// PlainYearMonth.add — large months overflow
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").add({ months: 4294967295 }), RangeError);

// PlainDate.add — same overflow path through calendarDateAdd
shouldThrow(() => Temporal.PlainDate.from("2020-01-01").add({ years: 4294967295 }), RangeError);
shouldThrow(() => Temporal.PlainDate.from("2020-01-01").add({ months: 4294967295 }), RangeError);

// Bug 2: plainYearMonthAdd ISO path incorrectly routed through ICU bridge, which
// clamps extreme boundary dates silently. -271821 is Temporal's minimum year;
// ICU's calendar doesn't support it and produces wrong results instead of RangeError.

// Near-minimum boundary: results that push internal ISO date out of range → RangeError
shouldThrow(() => Temporal.PlainYearMonth.from("-271821-04").add({ months: 1 }), RangeError);

// Near-maximum boundary
shouldThrow(() => Temporal.PlainYearMonth.from("+275760-09").add({ months: 1 }), RangeError);

// Valid boundary operations still work
shouldBe(Temporal.PlainYearMonth.from("-271821-04").toString(), "-271821-04");
shouldBe(Temporal.PlainYearMonth.from("+275760-09").toString(), "+275760-09");

// Sanity: normal values still work
shouldBe(Temporal.PlainYearMonth.from("2020-01").add({ years: 1 }).toString(), "2021-01");
shouldBe(Temporal.PlainYearMonth.from("2020-01").add({ months: 13 }).toString(), "2021-02");
shouldBe(Temporal.PlainYearMonth.from("-271821-04").toString(), "-271821-04");
