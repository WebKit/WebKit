//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function shouldBeUndefined(a, msg) {
    if (a !== undefined) throw new Error(`${msg}: expected undefined, got ${a}`);
}

{
    const pd = new Temporal.PlainDate(2024, 3, 15, "gregory");
    shouldBe(pd.calendarId, "gregory", "gregory calendarId");
    shouldBe(pd.era, "ce", "gregory era CE");
    shouldBe(pd.eraYear, 2024, "gregory eraYear");
    shouldBe(pd.year, 2024, "gregory year");
}

{
    const pd = new Temporal.PlainDate(-43, 3, 15, "gregory");
    shouldBe(pd.era, "bce", "gregory era BCE");
    shouldBe(pd.eraYear, 44, "gregory BCE eraYear (44 BC)");
}

{
    // Reiwa era started 2019-05-01
    const reiwa = new Temporal.PlainDate(2024, 1, 1, "japanese");
    shouldBe(reiwa.era, "reiwa", "japanese era reiwa");
    shouldBe(reiwa.eraYear, 6, "reiwa year 6 (2024)");
}

{
    // Heisei era: 1989-01-08 to 2019-04-30
    const heisei = new Temporal.PlainDate(2018, 6, 1, "japanese");
    shouldBe(heisei.era, "heisei", "japanese era heisei");
    shouldBe(heisei.eraYear, 30, "heisei year 30 (2018)");
}

{
    const pd = new Temporal.PlainDate(2024, 1, 1, "buddhist");
    shouldBe(pd.era, "be", "buddhist era");
    shouldBe(pd.eraYear, 2567, "buddhist year 2567 (2024 CE)");
}

{
    const pd = Temporal.PlainDate.from("2024-03-15");
    shouldBeUndefined(pd.era, "iso8601 era is undefined");
    shouldBeUndefined(pd.eraYear, "iso8601 eraYear is undefined");
}

{
    const pd = new Temporal.PlainDate(2024, 2, 10, "chinese");
    shouldBeUndefined(pd.era, "chinese era is undefined");
    shouldBeUndefined(pd.eraYear, "chinese eraYear is undefined");
    shouldBe(pd.calendarId, "chinese", "chinese calendarId");
}

{
    const pd = new Temporal.PlainDate(2024, 1, 1, "hebrew");
    shouldBe(pd.era, "am", "hebrew era");
    shouldBe(pd.calendarId, "hebrew", "hebrew calendarId");
}

{
    const pd = new Temporal.PlainDate(2024, 1, 1, "roc");
    shouldBe(pd.era, "roc", "roc era");
}

{
    const pd = new Temporal.PlainDate(2024, 1, 1, "persian");
    shouldBe(pd.era, "ap", "persian era");
}

{
    const pd = new Temporal.PlainDate(2024, 1, 1, "islamic-umalqura");
    shouldBe(pd.era, "ah", "islamic era");
}

{
    // February 2024 in gregory: 29 days (leap year)
    const feb = new Temporal.PlainDate(2024, 2, 1, "gregory");
    shouldBe(feb.daysInMonth, 29, "gregory Feb 2024 has 29 days");

    // February 2023 in gregory: 28 days
    const feb23 = new Temporal.PlainDate(2023, 2, 1, "gregory");
    shouldBe(feb23.daysInMonth, 28, "gregory Feb 2023 has 28 days");
}

{
    const leap = new Temporal.PlainDate(2024, 1, 1, "gregory");
    shouldBe(leap.inLeapYear, true, "2024 is leap year in gregory");

    const nonLeap = new Temporal.PlainDate(2023, 1, 1, "gregory");
    shouldBe(nonLeap.inLeapYear, false, "2023 is not leap year");
}

{
    const pd = new Temporal.PlainDate(2024, 3, 15, "gregory");
    shouldBe(pd.calendarId, "gregory", "constructor calendar argument");
    shouldBe(pd.era, "ce", "constructor gregory era");
}

{
    const pd = new Temporal.PlainDate(2024, 3, 15, "Gregory");
    shouldBe(pd.calendarId, "gregory", "calendar is case-insensitive");
}
