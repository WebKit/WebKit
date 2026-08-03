//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

const icuVersion = $vm.icuVersion();

// dayOfYear returns calendar-native day (chinese year starts different day than ISO).
{
    const chinese1969 = Temporal.PlainDate.from({ year: 1969, month: 1, day: 1, calendar: "chinese" });
    shouldBe(chinese1969.dayOfYear, 1, "chinese year 1969 M01 D01 -> dayOfYear=1");
    const pdt = Temporal.PlainDateTime.from({ year: 1969, month: 1, day: 1, hour: 12, calendar: "chinese" });
    shouldBe(pdt.dayOfYear, 1, "PDT chinese year 1969 M01 D01 -> dayOfYear=1");
    const zdt = Temporal.ZonedDateTime.from({ year: 1969, month: 1, day: 1, hour: 12, timeZone: "UTC", calendar: "chinese" });
    shouldBe(zdt.dayOfYear, 1, "ZDT chinese year 1969 M01 D01 -> dayOfYear=1");
}

// hebrew M13 and non-M05L leap month codes are invalid.
for (const cal of ["hebrew"]) {
    for (const mc of ["M13", "M01L", "M03L", "M06L", "M12L"]) {
        let threw = false;
        try { Temporal.PlainDate.from({ year: 5779, monthCode: mc, day: 1, calendar: cal }); }
        catch (e) { threw = e instanceof RangeError; }
        shouldBe(threw, true, `${cal} ${mc} -> RangeError`);
    }
    // M05L (Adar I) is valid in hebrew leap years.
    const validAdarI = Temporal.PlainDate.from({ year: 5779, monthCode: "M05L", day: 1, calendar: "hebrew" });
    shouldBe(validAdarI.monthCode, "M05L", "hebrew year 5779 M05L is valid");
}

// lunisolar leap-month year addition constrains monthCode to base month.
{
    // Chinese 1938 M07L D30. Chinese 1939 has no M07L, so +1y constrains to M07 D29 (M07 has 29 days in 1939).
    const start = Temporal.PlainDate.from({ year: 1938, monthCode: "M07L", day: 30, calendar: "chinese" });
    const plusOne = start.add(new Temporal.Duration(1));
    shouldBe(plusOne.monthCode, "M07", "chinese M07L +1y constrains to M07 in non-leap year");
    shouldBe(plusOne.day, 29, "chinese M07 D30 constrains to D29 (M07 chinese 1939 = 29 days)");
}

// lunisolar month/monthCode conflict check.
{
    // In chinese 2020 (has M04L), monthCode M05 is not at ordinal 12. Conflict.
    let threw = false;
    try { Temporal.PlainDate.from({ calendar: "chinese", year: 2020, monthCode: "M05", month: 12, day: 1 }); }
    catch (e) { threw = e instanceof RangeError; }
    shouldBe(threw, true, "chinese M05/month=12 conflict -> RangeError");
    // Correct ordinal works. ICU < 78 places 2020's leap month after M06 instead of M04
    // (verified wrong against icu4x's china_data.rs, which gives M04L; rdar://182753821),
    // making M05 ordinal 5 there instead of 6, so the call below throws — skip entirely
    // on affected ICU versions rather than just the assertion.
    if (icuVersion >= 78) {
        const ok = Temporal.PlainDate.from({ calendar: "chinese", year: 2020, monthCode: "M05", month: 6, day: 1 });
        shouldBe(ok.monthCode, "M05", "chinese M05/month=6 accepted");
    }
}

// PMD leap-month with year-from-options-bag falls back to reference year.
{
    // Chinese year 1651 has M01L (ICU4C). PMD should carry the reference year (1972), not 1651.
    const pd = Temporal.PlainDate.from({ calendar: "chinese", year: 1651, monthCode: "M01L", day: 29 });
    if (pd.monthCode === "M01L" && pd.day === 29) {
        const pmd = Temporal.PlainMonthDay.from({ calendar: "chinese", year: 1651, monthCode: "M01L", day: 29 });
        const pmdYear = Number(pmd.toString().split("-")[0]);
        shouldBe(pmdYear, 1972, "PMD chinese M01L D29 uses reference year 1972");
    }
}

// with() on hebrew year that doesn't have same leap distribution — monthCode preserved,
// month adjusts to new year's ordinal.
{
    const start = new Temporal.PlainDate(2024, 8, 8, "hebrew"); // hebrew 5784
    const changed = start.with({ year: 5783 });
    shouldBe(changed.year, 5783, "hebrew with year=5783");
    shouldBe(changed.monthCode, "M11", "hebrew monthCode M11 preserved");
    // The ordinal adjusts to match the new year's leap-month layout.
    shouldBe(typeof changed.month, "number", "month is numeric");
}

// REGRESSION: at extreme chinese/dangi years (beyond the ±10000 astronomical-reliability
// threshold), construction clamps to a representable ISO date without throwing, but reading
// any field back off the constructed object throws instead of returning the clamped values.
for (const cal of ["chinese", "dangi"]) {
    for (const year of [100000, -100000]) {
        const pd = Temporal.PlainDate.from({ calendar: cal, year, month: 1, day: 1 });
        shouldBe(typeof pd.year, "number", `${cal} year=${year} .year must not throw`);
        shouldBe(typeof pd.month, "number", `${cal} year=${year} .month must not throw`);
        shouldBe(typeof pd.day, "number", `${cal} year=${year} .day must not throw`);
        shouldBe(typeof pd.monthCode, "string", `${cal} year=${year} .monthCode must not throw`);
    }
}
