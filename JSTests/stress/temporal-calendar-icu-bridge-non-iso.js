//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

function shouldThrowRangeError(callback, label) {
    try {
        callback();
    } catch (error) {
        if (error instanceof RangeError)
            return;
        throw error;
    }
    throw new Error(`${label}: expected RangeError`);
}

function shouldHaveCalendarDateFields(date, expectedYear, expectedEra, expectedEraYear, expectedMonth, expectedDay, label) {
    shouldBe(date.year, expectedYear, `${label} year`);
    shouldBe(date.era, expectedEra, `${label} era`);
    shouldBe(date.eraYear, expectedEraYear, `${label} eraYear`);
    shouldBe(date.month, expectedMonth, `${label} month`);
    shouldBe(date.monthCode, `M0${expectedMonth}`, `${label} monthCode`);
    shouldBe(date.day, expectedDay, `${label} day`);
}

function shouldHaveISODate(date, expectedMonth, expectedDay, label) {
    const isoDate = date.withCalendar("iso8601");
    shouldBe(isoDate.year, 1500, `${label} ISO year`);
    shouldBe(isoDate.month, expectedMonth, `${label} ISO month`);
    shouldBe(isoDate.day, expectedDay, `${label} ISO day`);
}

function shouldRoundTripGregorianDate(date, createFromFields, expectedYear, expectedEra, expectedEraYear, expectedMonth, label) {
    shouldHaveCalendarDateFields(date, expectedYear, expectedEra, expectedEraYear, expectedMonth, 1, label);

    const result = date.with({ day: 2 });
    shouldHaveISODate(result, expectedMonth, 2, `${label} after with`);
    shouldHaveCalendarDateFields(result, expectedYear, expectedEra, expectedEraYear, expectedMonth, 2, `${label} after with`);

    const roundTrip = createFromFields({
        year: date.year,
        era: date.era,
        eraYear: date.eraYear,
        monthCode: date.monthCode,
        day: date.day,
        calendar: date.calendarId,
    });
    shouldHaveISODate(roundTrip, expectedMonth, 1, `${label} field round-trip`);
    shouldHaveCalendarDateFields(roundTrip, expectedYear, expectedEra, expectedEraYear, expectedMonth, 1, `${label} field round-trip`);
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

// ROC and Buddhist use proleptic Gregorian date fields before 1582.
for (const calendar of ["roc", "buddhist"]) {
    const expectedYear = calendar === "roc" ? -411 : 2043;
    const expectedEra = calendar === "roc" ? "broc" : "be";
    const expectedEraYear = calendar === "roc" ? 412 : 2043;
    for (const month of [1, 2]) {
        const isoMonth = `0${month}`;
        const dates = [
            ["PlainDate", Temporal.PlainDate.from(`1500-${isoMonth}-01`).withCalendar(calendar), fields => Temporal.PlainDate.from(fields)],
            ["PlainDateTime", Temporal.PlainDateTime.from(`1500-${isoMonth}-01T12:00`).withCalendar(calendar), fields => Temporal.PlainDateTime.from({ ...fields, hour: 12 })],
            ["ZonedDateTime", Temporal.ZonedDateTime.from(`1500-${isoMonth}-01T12:00Z[UTC]`).withCalendar(calendar), fields => Temporal.ZonedDateTime.from({ ...fields, hour: 12, timeZone: "UTC" })],
        ];
        for (const [type, date, createFromFields] of dates) {
            const label = `${calendar} ${type} 1500-${isoMonth}`;
            shouldRoundTripGregorianDate(date, createFromFields, expectedYear, expectedEra, expectedEraYear, month, label);
        }
    }
}

// ROC year 0 is the year before ROC year 1.
for (const [isoDate, year, era, eraYear] of [
    ["1911-12-31", 0, "broc", 1],
    ["1912-01-01", 1, "roc", 1],
]) {
    const date = Temporal.PlainDate.from(isoDate).withCalendar("roc");
    shouldBe(date.year, year, `${isoDate} ROC year`);
    shouldBe(date.era, era, `${isoDate} ROC era`);
    shouldBe(date.eraYear, eraYear, `${isoDate} ROC eraYear`);
    const roundTrip = Temporal.PlainDate.from({ year, era, eraYear, monthCode: date.monthCode, day: date.day, calendar: "roc" });
    shouldBe(roundTrip.withCalendar("iso8601").toString(), isoDate, `${isoDate} ROC round-trip`);
}

// Buddhist has one era and its arithmetic year may be non-positive.
for (const [year, isoDate] of [
    [-1, "-000544-01-01"],
    [0, "-000543-01-01"],
    [1, "-000542-01-01"],
]) {
    const date = Temporal.PlainDate.from({ year, era: "be", eraYear: year, monthCode: "M01", day: 1, calendar: "buddhist" });
    shouldBe(date.withCalendar("iso8601").toString(), isoDate, `Buddhist year ${year} ISO date`);
    shouldBe(date.year, year, `Buddhist year ${year}`);
    shouldBe(date.era, "be", `Buddhist year ${year} era`);
    shouldBe(date.eraYear, year, `Buddhist year ${year} eraYear`);
}

shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 0, era: "roc", eraYear: 1, monthCode: "M01", day: 1, calendar: "roc" }), "inconsistent ROC year and eraYear");
shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 0, era: "be", eraYear: 1, monthCode: "M01", day: 1, calendar: "buddhist" }), "inconsistent Buddhist year and eraYear");

for (const [calendar, year] of [["roc", -411], ["buddhist", 2043]]) {
    const constrainedDay = Temporal.PlainDate.from({ year, month: 2, day: 29, calendar }, { overflow: "constrain" });
    shouldBe(constrainedDay.withCalendar("iso8601").toString(), "1500-02-28", `${calendar} day constrain`);
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year, month: 2, day: 29, calendar }, { overflow: "reject" }), `${calendar} day reject`);

    const constrainedMonth = Temporal.PlainDate.from({ year, month: 13, day: 1, calendar }, { overflow: "constrain" });
    shouldBe(constrainedMonth.withCalendar("iso8601").toString(), "1500-12-01", `${calendar} month constrain`);
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year, month: 13, day: 1, calendar }, { overflow: "reject" }), `${calendar} month reject`);
}

// The exact Temporal PlainDate limits support stable calendar-field round-trips.
for (const [isoDate, calendar, year, era, eraYear] of [
    ["-271821-04-19", "roc", -273732, "broc", 273733],
    ["-271821-04-19", "buddhist", -271278, "be", -271278],
    ["+275760-09-13", "roc", 273849, "roc", 273849],
    ["+275760-09-13", "buddhist", 276303, "be", 276303],
]) {
    const date = Temporal.PlainDate.from(isoDate).withCalendar(calendar);
    shouldBe(date.year, year, `${calendar} ${isoDate} year`);
    shouldBe(date.era, era, `${calendar} ${isoDate} era`);
    shouldBe(date.eraYear, eraYear, `${calendar} ${isoDate} eraYear`);
    const roundTrip = Temporal.PlainDate.from({ year, era, eraYear, monthCode: date.monthCode, day: date.day, calendar });
    shouldBe(roundTrip.withCalendar("iso8601").toString(), isoDate, `${calendar} ${isoDate} round-trip`);
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
