//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, ErrCtor, label) {
    let threw;
    try { fn(); } catch (e) { threw = e.constructor; }
    if (threw !== ErrCtor)
        throw new Error(`${label}: expected ${ErrCtor.name}, got ${threw ? threw.name : "no throw"}`);
}

// Hebrew: only M05L is valid.
for (const mc of ["M01L", "M02L", "M03L", "M04L", "M06L", "M07L", "M08L", "M09L", "M10L", "M11L", "M12L"]) {
    shouldThrow(() => Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, monthCode: mc, day: 1 }),
        RangeError, `hebrew PD ${mc}`);
    shouldThrow(() => Temporal.PlainYearMonth.from({ calendar: "hebrew", year: 5784, monthCode: mc }),
        RangeError, `hebrew PYM ${mc}`);
    shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: "hebrew", monthCode: mc, day: 1 }),
        RangeError, `hebrew PMD ${mc}`);
}

// Chinese/Dangi: M01L..M12L only, no M13.
for (const cal of ["chinese", "dangi"]) {
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 2024, monthCode: "M13", day: 1 }),
        RangeError, `${cal} M13`);
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 2024, monthCode: "M13L", day: 1 }),
        RangeError, `${cal} M13L`);
}

// Solar calendars: no leap monthCodes.
for (const cal of ["gregory", "buddhist", "indian", "japanese", "persian", "roc"]) {
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 2024, monthCode: "M05L", day: 1 }),
        RangeError, `${cal} M05L`);
}

// Coptic/Ethiopic/Ethioaa: M13 valid; MnnL and M14+ invalid.
for (const cal of ["coptic", "ethiopic", "ethioaa"]) {
    Temporal.PlainDate.from({ calendar: cal, year: 1740, monthCode: "M13", day: 1 });
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 1740, monthCode: "M14", day: 1 }),
        RangeError, `${cal} M14`);
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 1740, monthCode: "M05L", day: 1 }),
        RangeError, `${cal} M05L`);
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, year: 1740, monthCode: "M13L", day: 1 }),
        RangeError, `${cal} M13L`);
}

// Chinese 2020 (no M02L, ~skip-backward~ → M02, ordinal 2).
Temporal.PlainDate.from({ calendar: "chinese", year: 2020, month: 2, monthCode: "M02L", day: 1 });
shouldThrow(() => Temporal.PlainDate.from({ calendar: "chinese", year: 2020, month: 3, monthCode: "M02L", day: 1 }),
    RangeError, "chinese 2020 M02L constrain->M02 vs m=3");
// Hebrew 5783 (non-leap, ~skip-forward~ → M06, ordinal 6).
Temporal.PlainDate.from({ calendar: "hebrew", year: 5783, month: 6, monthCode: "M05L", day: 1 });
shouldThrow(() => Temporal.PlainDate.from({ calendar: "hebrew", year: 5783, month: 5, monthCode: "M05L", day: 1 }),
    RangeError, "hebrew 5783 M05L constrain->M06 vs m=5");
