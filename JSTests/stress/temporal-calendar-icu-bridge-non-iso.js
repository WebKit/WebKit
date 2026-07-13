//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
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
