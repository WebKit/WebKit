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

// MonthDay with japanese calendar — era-aware with()
{
    const md = Temporal.PlainMonthDay.from({ monthCode: "M01", day: 1, calendar: "japanese" });
    const result = md.with({ day: 15 });
    shouldBe(result.day, 15);
    shouldBe(result.monthCode, "M01");
}

// era+eraYear + monthCode, no year: internally monthDayFromFields computes a non-zero
// ecmaReferenceYear but must NOT trigger the year-consistency check — that only applies
// to user-provided year. Regresses if CalendarFields.cpp passes ecmaReferenceYear instead of 0.
{
    const md = Temporal.PlainMonthDay.from({ era: "showa", eraYear: 50, monthCode: "M06", day: 15, calendar: "japanese" });
    shouldBe(md.monthCode, "M06");
    shouldBe(md.day, 15);
}

// era+eraYear + monthCode + consistent year → OK
{
    const md = Temporal.PlainMonthDay.from({ era: "showa", eraYear: 50, monthCode: "M06", day: 15, year: 1975, calendar: "japanese" });
    shouldBe(md.monthCode, "M06");
    shouldBe(md.day, 15);
}

// era+eraYear + monthCode + inconsistent year → RangeError
{
    shouldThrow(() => Temporal.PlainMonthDay.from({ era: "showa", eraYear: 50, monthCode: "M06", day: 15, year: 2000, calendar: "japanese" }), RangeError);
}

// iso8601 (no eras) still works
{
    const md = new Temporal.PlainMonthDay(3, 14);
    const result = md.with({ day: 20 });
    shouldBe(result.monthCode, "M03");
    shouldBe(result.day, 20);
}
