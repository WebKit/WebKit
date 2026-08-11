//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, ErrCtor, label) {
    let threw;
    try { fn(); } catch (e) { threw = e.constructor; }
    if (threw !== ErrCtor)
        throw new Error(`${label}: expected ${ErrCtor.name}, got ${threw ? threw.name : "no throw"}`);
}

// PlainDate.from: TypeError before RangeError.
{
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "gregory", monthCode: "M05", month: 6, day: 1 }),
        TypeError, "PD gregory missing year -> TypeError before conflict");
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "gregory", year: 2020, day: 32 }),
        TypeError, "PD gregory missing month -> TypeError");
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "gregory", year: 2020, monthCode: "M05", month: 6 }),
        TypeError, "PD gregory missing day -> TypeError");
    // era without eraYear -> TypeError.
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "gregory", era: "ce", monthCode: "M05", month: 6, day: 1 }),
        TypeError, "PD gregory era without eraYear -> TypeError");
    // Range check still fires after all types valid.
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "gregory", year: 2020, monthCode: "M05", month: 6, day: 1 }),
        RangeError, "PD gregory month/monthCode conflict -> RangeError");
}

// PlainYearMonth.from: TypeError before RangeError.
{
    shouldThrow(() => Temporal.PlainYearMonth.from({ calendar: "gregory", monthCode: "M05", month: 6 }),
        TypeError, "PYM gregory missing year -> TypeError");
    shouldThrow(() => Temporal.PlainYearMonth.from({ calendar: "gregory", year: 2020 }),
        TypeError, "PYM gregory missing month -> TypeError");
    shouldThrow(() => Temporal.PlainYearMonth.from({ calendar: "gregory", year: 2020, monthCode: "M05", month: 6 }),
        RangeError, "PYM gregory month/monthCode conflict -> RangeError");
}

// PlainMonthDay.from: month+monthCode requires year for calendar-year disambiguation.
{
    shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: "gregory", monthCode: "M04", month: 5, day: 1 }),
        TypeError, "PMD gregory month+monthCode without year -> TypeError");
    shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: "gregory", year: 2020, day: 15 }),
        TypeError, "PMD gregory missing month/monthCode -> TypeError");
}

// ZonedDateTime.with: era without eraYear -> TypeError.
{
    const z = Temporal.ZonedDateTime.from({ calendar: "gregory", timeZone: "UTC", year: 2020, month: 5, day: 15, hour: 12 });
    shouldThrow(() => z.with({ era: "ce", monthCode: "M05", month: 6 }),
        TypeError, "ZDT.with era without eraYear -> TypeError");
    shouldThrow(() => z.with({ monthCode: "M05", month: 6 }),
        RangeError, "ZDT.with month/monthCode conflict -> RangeError");
}
