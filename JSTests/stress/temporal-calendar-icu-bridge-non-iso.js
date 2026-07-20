//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

function shouldThrow(func, errorType, label) {
    try {
        func();
    } catch (error) {
        if (error instanceof errorType)
            return;
        throw new Error(`${label}: expected ${errorType.name}, got ${error.constructor.name}`);
    }
    throw new Error(`${label}: expected ${errorType.name}, but no exception was thrown`);
}

// Buddhist: `year` is BE (Gregorian + 543). BE 2567 = Gregorian 2024.
{
    const pd = Temporal.PlainDate.from({year:2567, month:1, day:1, calendar:"buddhist"});
    shouldBe(pd.toString(), "2024-01-01[u-ca=buddhist]", "buddhist year:2567 -> ISO 2024");
    shouldBe(pd.year, 2567, "buddhist .year = BE year");
    shouldBe(pd.eraYear, 2567, "buddhist .eraYear = BE year");
    shouldBe(pd.withCalendar("iso8601").year, 2024, "buddhist -> iso8601 year");
    shouldBe(pd.era, "be", "buddhist .era");
}
{
    const bud = Temporal.PlainDate.from("2024-01-01").withCalendar("buddhist");
    shouldBe(bud.year, 2567, "ISO 2024 -> buddhist year");
}
{
    const pd = Temporal.PlainDate.from({era:"be", eraYear:2567, month:1, day:1, calendar:"buddhist"});
    shouldBe(pd.year, 2567, "buddhist era:be eraYear:2567 -> year");
    shouldBe(pd.toString(), "2024-01-01[u-ca=buddhist]", "buddhist era -> ISO");
}

// Coptic `am` era: AM 1740 M01 D01 = ISO 2023-09-12.
{
    const pd = Temporal.PlainDate.from({era:"am", eraYear:1740, month:1, day:1, calendar:"coptic"});
    shouldBe(pd.year, 1740, "coptic am eraYear:1740 -> year");
    shouldBe(pd.toString(), "2023-09-12[u-ca=coptic]", "coptic am 1740 -> ISO 2023-09-12");
    shouldBe(pd.era, "am", "coptic .era");
}
{
    const pd = Temporal.PlainDate.from({year:1740, month:1, day:1, calendar:"coptic"});
    shouldBe(pd.toString(), "2023-09-12[u-ca=coptic]", "coptic year:1740 -> ISO 2023-09-12");
}

// Ethiopic `am` era: AM 2016 M01 D01 = ISO 2023-09-12.
{
    const pd = Temporal.PlainDate.from({era:"am", eraYear:2016, month:1, day:1, calendar:"ethiopic"});
    shouldBe(pd.toString(), "2023-09-12[u-ca=ethiopic]", "ethiopic am 2016 -> ISO 2023-09-12");
    shouldBe(pd.year, 2016, "ethiopic .year");
    shouldBe(pd.era, "am", "ethiopic .era");
    shouldBe(pd.eraYear, 2016, "ethiopic .eraYear");
}

// Ethiopic `aa` era: ISO 0001-01-01 is AA 5493 M05 D08.
{
    const historical = Temporal.PlainDate.from({era:"aa", eraYear:-1, monthCode:"M02", day:21, calendar:"ethiopic"}, {overflow:"reject"});
    shouldBe(historical.toString(), "-005494-09-05[u-ca=ethiopic]", "ethiopic aa -1 -> ISO -5494-09-05");
    shouldBe(historical.year, -5501, "ethiopic aa -1 -> year");
    shouldBe(historical.era, "aa", "ethiopic aa -1 -> era");
    shouldBe(historical.eraYear, -1, "ethiopic aa -1 -> eraYear");
    const pd = Temporal.PlainDate.from("0001-01-01[u-ca=ethiopic]");
    shouldBe(pd.year, -7, "ethiopic historical .year");
    shouldBe(pd.era, "aa", "ethiopic historical .era");
    shouldBe(pd.eraYear, 5493, "ethiopic historical .eraYear");
}

// Ethioaa has one era, so its arithmetic year is the same as its era year.
const modernEthioaaYear = 7518; // ICU 74 reports extended year 2018 here; ICU 78 reports 7518.
for (const year of [-1, 0, 1, modernEthioaaYear]) {
    const fromEra = Temporal.PlainDate.from({era:"aa", eraYear:year, month:1, day:1, calendar:"ethioaa"}, {overflow:"reject"});
    shouldBe(fromEra.year, year, `ethioaa eraYear:${year} -> year`);
    shouldBe(fromEra.era, "aa", `ethioaa eraYear:${year} -> era`);
    shouldBe(fromEra.eraYear, year, `ethioaa eraYear:${year} round-trip`);

    const fromYear = Temporal.PlainDate.from({year, month:1, day:1, calendar:"ethioaa"}, {overflow:"reject"});
    shouldBe(fromYear.toString(), fromEra.toString(), `ethioaa year:${year} matches eraYear:${year}`);

    const consistent = Temporal.PlainDate.from({year, era:"aa", eraYear:year, month:1, day:1, calendar:"ethioaa"}, {overflow:"reject"});
    shouldBe(consistent.toString(), fromEra.toString(), `ethioaa consistent year and eraYear ${year}`);
    shouldBe(fromEra.with({day:2}).year, year, `ethioaa year ${year} survives with()`);
}

// Inconsistent year and eraYear must be rejected across distinct calendar field-resolution paths.
shouldThrow(() => Temporal.PlainDate.from({year:-1, era:"aa", eraYear:0, month:1, day:1, calendar:"ethioaa"}), RangeError, "ethioaa PlainDate inconsistent non-positive years");
shouldThrow(() => Temporal.PlainYearMonth.from({year:0, era:"aa", eraYear:-1, month:1, calendar:"ethioaa"}), RangeError, "ethioaa PlainYearMonth inconsistent non-positive years");
shouldThrow(() => Temporal.PlainMonthDay.from({year:-1, era:"aa", eraYear:0, monthCode:"M01", day:1, calendar:"ethioaa"}), RangeError, "ethioaa PlainMonthDay inconsistent non-positive years");

{
    const date = Temporal.PlainDate.from({era:"aa", eraYear:-1, month:1, day:1, calendar:"ethioaa"});
    shouldThrow(() => date.with({year:0, era:"aa", eraYear:-1}), RangeError, "ethioaa PlainDate.with inconsistent non-positive years");
}

// Japanese pre-1582: proleptic Gregorian. 1500 is Julian-leap but not Gregorian-leap.
{
    const pd = Temporal.PlainDate.from({era:"ce", eraYear:1500, month:6, day:15, calendar:"japanese"});
    shouldBe(pd.daysInYear, 365, "japanese ce 1500 daysInYear (Gregorian)");
    shouldBe(pd.inLeapYear, false, "japanese ce 1500 inLeapYear (Gregorian)");
    const feb = Temporal.PlainDate.from({era:"ce", eraYear:1500, month:2, day:1, calendar:"japanese"});
    shouldBe(feb.daysInMonth, 28, "japanese ce 1500 Feb has 28 days");
}
{
    const pd = Temporal.PlainDate.from({era:"ce", eraYear:1600, month:2, day:29, calendar:"japanese"});
    shouldBe(pd.inLeapYear, true, "japanese ce 1600 inLeapYear");
    shouldBe(pd.daysInYear, 366, "japanese ce 1600 daysInYear");
}
