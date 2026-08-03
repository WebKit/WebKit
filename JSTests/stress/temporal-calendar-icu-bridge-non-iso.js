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

function shouldBeDate(actual, year, month, day, label) {
    shouldBe(actual.year, year, `${label} year`);
    shouldBe(actual.month, month, `${label} month`);
    shouldBe(actual.day, day, `${label} day`);
}

function shouldRoundTripDuration(start, end, largestUnit, expected, label) {
    const forward = start.until(end, {largestUnit});
    shouldBe(forward.toString(), expected, `${label} forward difference`);
    shouldBe(start.add(forward).equals(end), true, `${label} forward inverse`);
    const reverse = end.until(start, {largestUnit});
    shouldBe(reverse.toString(), `-${expected}`, `${label} reverse difference`);
    shouldBe(end.add(reverse).equals(start), true, `${label} reverse inverse`);
}

function shouldHaveCalendarDateFields(date, year, era, eraYear, month, day, label) {
    shouldBe(date.year, year, `${label} year`);
    shouldBe(date.era, era, `${label} era`);
    shouldBe(date.eraYear, eraYear, `${label} eraYear`);
    shouldBe(date.month, month, `${label} month`);
    shouldBe(date.monthCode, `M${String(month).padStart(2, "0")}`, `${label} monthCode`);
    shouldBe(date.day, day, `${label} day`);
}

// Japanese `year` is always the proleptic-Gregorian ISO year. Era fields remain calendar-native.
for (const [isoDate, era, eraYear] of [
    ["-271821-04-19", "bce", 271822],
    ["-000001-01-01", "bce", 2],
    ["0000-01-01", "bce", 1],
    ["0001-01-01", "ce", 1],
    ["1000-01-01", "ce", 1000],
    ["1500-02-28", "ce", 1500],
    ["1868-10-22", "ce", 1868],
    ["1872-12-31", "ce", 1872],
    ["1873-01-01", "meiji", 6],
    ["1912-07-29", "meiji", 45],
    ["1912-07-30", "taisho", 1],
    ["1926-12-24", "taisho", 15],
    ["1926-12-25", "showa", 1],
    ["1989-01-07", "showa", 64],
    ["1989-01-08", "heisei", 1],
    ["2019-04-30", "heisei", 31],
    ["2019-05-01", "reiwa", 1],
    ["+275760-09-13", "reiwa", 273742],
]) {
    const iso = Temporal.PlainDate.from(isoDate);
    const date = iso.withCalendar("japanese");
    shouldHaveCalendarDateFields(date, iso.year, era, eraYear, iso.month, iso.day, `japanese ${isoDate}`);

    const roundTrip = Temporal.PlainDate.from({
        calendar: "japanese",
        year: date.year,
        era: date.era,
        eraYear: date.eraYear,
        monthCode: date.monthCode,
        day: date.day,
    });
    shouldBe(roundTrip.withCalendar("iso8601").toString(), isoDate, `japanese ${isoDate} field round-trip`);
}

for (const [fields, isoDate, era, eraYear] of [
    [{ year: -1, era: "bce", eraYear: 2, month: 1, day: 1 }, "-000001-01-01", "bce", 2],
    [{ year: 1000, era: "ce", eraYear: 1000, month: 1, day: 1 }, "1000-01-01", "ce", 1000],
    [{ year: 1873, era: "meiji", eraYear: 6, month: 1, day: 1 }, "1873-01-01", "meiji", 6],
    [{ year: 2019, era: "reiwa", eraYear: 1, month: 5, day: 1 }, "2019-05-01", "reiwa", 1],
]) {
    const date = Temporal.PlainDate.from({ calendar: "japanese", ...fields }, { overflow: "reject" });
    shouldBe(date.withCalendar("iso8601").toString(), isoDate, `japanese ${isoDate} from fields`);
    shouldHaveCalendarDateFields(date, fields.year, era, eraYear, fields.month, fields.day, `japanese ${isoDate} from fields`);

    const updated = date.with({ day: 2 }, { overflow: "reject" });
    shouldHaveCalendarDateFields(updated, fields.year, era, eraYear, fields.month, 2, `japanese ${isoDate} with`);
}

// A Japanese year-only property bag uses the same ISO year exposed by the getter.
for (const [year, month, day, era, eraYear] of [
    [1000, 2, 28, "ce", 1000],
    [1872, 12, 31, "ce", 1872],
    [1873, 1, 1, "meiji", 6],
]) {
    const date = Temporal.PlainDate.from({ calendar: "japanese", year, month, day }, { overflow: "reject" });
    shouldBe(date.withCalendar("iso8601").toString(), `${String(year).padStart(4, "0")}-${String(month).padStart(2, "0")}-${String(day).padStart(2, "0")}`, `japanese year-only ${year} ISO date`);
    shouldHaveCalendarDateFields(date, year, era, eraYear, month, day, `japanese year-only ${year}`);
}

const japaneseYearMonth = Temporal.PlainYearMonth.from({ calendar: "japanese", year: 1000, month: 2 }, { overflow: "reject" });
shouldBe(japaneseYearMonth.toString({ calendarName: "never" }), "1000-02-01", "japanese year-only PlainYearMonth ISO date");
shouldBe(japaneseYearMonth.year, 1000, "japanese year-only PlainYearMonth year");
shouldBe(japaneseYearMonth.month, 2, "japanese year-only PlainYearMonth month");
shouldBe(japaneseYearMonth.monthCode, "M02", "japanese year-only PlainYearMonth monthCode");

// Named Meiji era input uses proleptic Gregorian arithmetic before the 1873 cutover.
const negativeMeiji = Temporal.PlainDate.from({ calendar: "japanese", year: 1500, era: "meiji", eraYear: -367, month: 12, day: 31 }, { overflow: "reject" });
shouldBe(negativeMeiji.withCalendar("iso8601").toString(), "1500-12-31", "negative Meiji eraYear ISO date");
shouldBe(negativeMeiji.with({ era: "meiji", eraYear: -367, day: 30 }, { overflow: "reject" }).withCalendar("iso8601").toString(), "1500-12-30", "negative Meiji eraYear with");
shouldBe(Temporal.PlainDate.from({ calendar: "japanese", year: negativeMeiji.year, era: negativeMeiji.era, eraYear: negativeMeiji.eraYear, monthCode: negativeMeiji.monthCode, day: negativeMeiji.day }).withCalendar("iso8601").toString(), "1500-12-31", "negative Meiji eraYear round-trip");
shouldThrow(() => Temporal.PlainDate.from({ calendar: "japanese", year: 1501, era: "meiji", eraYear: -367, month: 12, day: 31 }), RangeError, "inconsistent negative Meiji year");
shouldBe(Temporal.PlainDate.from({ calendar: "japanese", era: "meiji", eraYear: -367, month: 2, day: 29 }).withCalendar("iso8601").toString(), "1500-02-28", "negative Meiji Gregorian February constrain");
shouldThrow(() => Temporal.PlainDate.from({ calendar: "japanese", era: "meiji", eraYear: -367, month: 2, day: 29 }, { overflow: "reject" }), RangeError, "negative Meiji Gregorian February reject");
shouldBe(Temporal.PlainDate.from({ calendar: "japanese", era: "meiji", eraYear: 0, month: 1, day: 1 }).withCalendar("iso8601").toString(), "1867-01-01", "Meiji eraYear zero");

for (const [era, month, day, isoDate, outputEra, outputEraYear] of [["meiji", 9, 8, "1868-09-08", "ce", 1868], ["taisho", 7, 30, "1912-07-30", "taisho", 1], ["showa", 12, 25, "1926-12-25", "showa", 1], ["heisei", 1, 8, "1989-01-08", "heisei", 1], ["reiwa", 5, 1, "2019-05-01", "reiwa", 1]]) {
    const date = Temporal.PlainDate.from({ calendar: "japanese", era, eraYear: 1, month, day }, { overflow: "reject" });
    shouldBe(date.withCalendar("iso8601").toString(), isoDate, `${era} transition ISO date`);
    shouldHaveCalendarDateFields(date, Number(isoDate.slice(0, 4)), outputEra, outputEraYear, month, day, `${era} transition`);
}

shouldThrow(() => Temporal.PlainDate.from({ calendar: "japanese", year: 999, era: "ce", eraYear: 1000, month: 1, day: 1 }), RangeError, "inconsistent Japanese CE year");
shouldThrow(() => Temporal.PlainDate.from({ calendar: "japanese", year: 2018, era: "reiwa", eraYear: 1, month: 5, day: 1 }), RangeError, "inconsistent Japanese Reiwa year");

for (const [type, date] of [
    ["PlainDateTime", Temporal.PlainDateTime.from("1000-01-01T12:00").withCalendar("japanese")],
    ["ZonedDateTime", Temporal.ZonedDateTime.from("1000-01-01T12:00Z[UTC]").withCalendar("japanese")],
]) {
    shouldHaveCalendarDateFields(date, 1000, "ce", 1000, 1, 1, `japanese ${type}`);
    shouldHaveCalendarDateFields(date.with({ day: 2 }), 1000, "ce", 1000, 1, 2, `japanese ${type} with`);
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

function shouldHaveISODate(date, expectedYear, expectedMonth, expectedDay, label) {
    const isoDate = date.withCalendar("iso8601");
    shouldBe(isoDate.year, expectedYear, `${label} ISO year`);
    shouldBe(isoDate.month, expectedMonth, `${label} ISO month`);
    shouldBe(isoDate.day, expectedDay, `${label} ISO day`);
}

function shouldRoundTripGregorianDate(date, createFromFields, expectedYear, expectedEra, expectedEraYear, expectedMonth, label) {
    shouldHaveCalendarDateFields(date, expectedYear, expectedEra, expectedEraYear, expectedMonth, 1, label);

    const result = date.with({ day: 2 });
    shouldHaveISODate(result, 1500, expectedMonth, 2, `${label} after with`);
    shouldHaveCalendarDateFields(result, expectedYear, expectedEra, expectedEraYear, expectedMonth, 2, `${label} after with`);

    const roundTrip = createFromFields({
        year: date.year,
        era: date.era,
        eraYear: date.eraYear,
        monthCode: date.monthCode,
        day: date.day,
        calendar: date.calendarId,
    });
    shouldHaveISODate(roundTrip, 1500, expectedMonth, 1, `${label} field round-trip`);
    shouldHaveCalendarDateFields(roundTrip, expectedYear, expectedEra, expectedEraYear, expectedMonth, 1, `${label} field round-trip`);
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

shouldThrow(() => Temporal.PlainDate.from({ year: 0, era: "roc", eraYear: 1, monthCode: "M01", day: 1, calendar: "roc" }), RangeError, "inconsistent ROC year and eraYear");
shouldThrow(() => Temporal.PlainDate.from({ year: 0, era: "be", eraYear: 1, monthCode: "M01", day: 1, calendar: "buddhist" }), RangeError, "inconsistent Buddhist year and eraYear");

for (const [calendar, fields] of [["japanese", { era: "meiji", eraYear: -367 }], ["roc", { year: -411 }], ["buddhist", { year: 2043 }]]) {
    const constrained = Temporal.PlainMonthDay.from({ calendar, ...fields, month: 2, day: 29 });
    shouldBe(constrained.toString(), `1972-02-28[u-ca=${calendar}]`, `${calendar} historical PlainMonthDay constrain and reference year`);
    shouldThrow(() => Temporal.PlainMonthDay.from({ calendar, ...fields, month: 2, day: 29 }, { overflow: "reject" }), RangeError, `${calendar} historical PlainMonthDay reject`);
    shouldBe(Temporal.PlainMonthDay.from({ calendar, monthCode: "M02", day: 29 }).toString(), `1972-02-29[u-ca=${calendar}]`, `${calendar} PlainMonthDay leap reference year`);
}
for (const [calendar, fields] of [["roc", { era: "roc", eraYear: 2147483647 }], ["roc", { era: "broc", eraYear: -2147483648 }], ["buddhist", { era: "be", eraYear: 2147483647 }], ["buddhist", { era: "be", eraYear: -2147483648 }]]) {
    shouldThrow(() => Temporal.ZonedDateTime.from(`2000-01-01T00:00[UTC][u-ca=${calendar}]`).with(fields), RangeError, `${calendar} ZonedDateTime.with checked year offset`);
}

for (const [calendar, year] of [["roc", -411], ["buddhist", 2043]]) {
    const constrainedDay = Temporal.PlainDate.from({ year, month: 2, day: 29, calendar }, { overflow: "constrain" });
    shouldBe(constrainedDay.withCalendar("iso8601").toString(), "1500-02-28", `${calendar} day constrain`);
    shouldThrow(() => Temporal.PlainDate.from({ year, month: 2, day: 29, calendar }, { overflow: "reject" }), RangeError, `${calendar} day reject`);

    const constrainedMonth = Temporal.PlainDate.from({ year, month: 13, day: 1, calendar }, { overflow: "constrain" });
    shouldBe(constrainedMonth.withCalendar("iso8601").toString(), "1500-12-01", `${calendar} month constrain`);
    shouldThrow(() => Temporal.PlainDate.from({ year, month: 13, day: 1, calendar }, { overflow: "reject" }), RangeError, `${calendar} month reject`);
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

// Month arithmetic uses calendar-native months, not ISO months.
const indianMonthDate = Temporal.PlainDate.from({year:1945, month:12, day:29, calendar:"indian"});
const indianAddedMonth = indianMonthDate.add({months:1});
shouldBeDate(indianAddedMonth, 1946, 1, 29, "indian add month");
shouldBeDate(indianAddedMonth.subtract({months:1}), 1945, 12, 29, "indian subtract month");

// Calendar-native add/until balances years and months, including month correction and day remainders.
for (const [calendar, label, startFields, endFields, largestUnit, expected] of [
    ["persian", "add/until", {year:1402, month:10, day:15}, {year:1404, month:1, day:15}, "years", "P1Y3M"],
    ["coptic", "add/until", {year:1739, month:11, day:15}, {year:1741, month:1, day:15}, "years", "P1Y3M"],
    ["persian", "end-of-month surpass", {year:1402, month:6, day:31}, {year:1402, month:7, day:30}, "months", "P30D"],
    ["persian", "month/day remainder", {year:1402, month:1, day:15}, {year:1402, month:3, day:20}, "months", "P2M5D"],
]) {
    const start = Temporal.PlainDate.from({...startFields, calendar});
    const end = Temporal.PlainDate.from({...endFields, calendar});
    shouldRoundTripDuration(start, end, largestUnit, expected, `${calendar} ${label}`);
}

// Constrain and reject apply after calendar-native month and leap-day year arithmetic.
for (const [label, unit, fields, expected] of [
    ["persian", "month", {year:1402, month:11, day:30, calendar:"persian"}, [1402, 12, 29]],
    ["coptic", "month", {year:1740, month:12, day:30, calendar:"coptic"}, [1740, 13, 5]],
    ["indian", "month", {year:1945, month:6, day:31, calendar:"indian"}, [1945, 7, 30]],
    ["persian", "year", {year:1399, month:12, day:30, calendar:"persian"}, [1400, 12, 29]],
    ["coptic", "year", {year:1739, month:13, day:6, calendar:"coptic"}, [1740, 13, 5]],
    ["indian", "year", {year:1946, month:1, day:31, calendar:"indian"}, [1947, 1, 30]],
]) {
    const pd = Temporal.PlainDate.from(fields);
    const duration = unit === "month" ? {months:1} : {years:1};
    shouldBeDate(pd.add(duration), ...expected, `${label} constrained ${unit}`);
    shouldThrow(() => pd.add(duration, {overflow:"reject"}), RangeError, `${label} rejected ${unit}`);
}

// Weeks and days are applied after calendar-native years and months for both signs.
const positiveMixedDate = Temporal.PlainDate.from({year:1402, month:12, day:29, calendar:"persian"});
shouldBeDate(positiveMixedDate.add({months:1, weeks:1, days:1}), 1403, 2, 6, "persian positive mixed duration");
const negativeMixedDate = Temporal.PlainDate.from({year:1403, month:1, day:29, calendar:"persian"});
shouldBeDate(negativeMixedDate.subtract({months:1, weeks:1, days:1}), 1402, 12, 21, "persian negative mixed duration");

// Ethiopic calendars use the same 13-month solar arithmetic as Coptic.
for (const calendar of ["ethiopic", "ethioaa"]) {
    const pd = Temporal.PlainDate.from({year:2015, month:13, day:5, calendar});
    shouldBeDate(pd.add({months:1}), 2016, 1, 5, `${calendar} add month`);
}

// A Gregorian-derived calendar keeps exact ISO proleptic-Gregorian arithmetic.
const gregorianMonthDate = Temporal.PlainDate.from("2024-01-31").withCalendar("gregory");
shouldBe(gregorianMonthDate.add({months:1}).withCalendar("iso8601").toString(), "2024-02-29", "gregory add month");

// Representative existing lunisolar arithmetic still enters intercalary months.
for (const [date, expectedMonthCode, expectedDay, calendar] of [
    [Temporal.PlainDate.from("2023-02-20").withCalendar("chinese"), "M02L", 1, "chinese"],
    [Temporal.PlainDate.from({year:5784, monthCode:"M05", day:15, calendar:"hebrew"}), "M05L", 15, "hebrew"],
]) {
    const added = date.add({months:1});
    shouldBe(added.monthCode, expectedMonthCode, `${calendar} add into leap month monthCode`);
    shouldBe(added.day, expectedDay, `${calendar} add into leap month day`);
}

// ICU must not silently clamp native month arithmetic at Temporal's representable limits.
shouldBe(Temporal.PlainDate.from("-271821-05-19").withCalendar("indian").add({months:-1}).withCalendar("iso8601").toString(), "-271821-04-19", "indian exact minimum boundary");
shouldBe(Temporal.PlainDate.from("-271821-04-19").withCalendar("indian").add({months:1}).withCalendar("iso8601").toString(), "-271821-05-19", "indian add from exact minimum boundary");
shouldThrow(() => Temporal.PlainDate.from("-271821-04-20").withCalendar("indian").add({months:-1}), RangeError, "indian minimum boundary");
shouldThrow(() => Temporal.PlainDate.from("-271821-05-20").withCalendar("indian").add({months:-2}), RangeError, "indian multi-month partial minimum clamp");
shouldThrow(() => Temporal.PlainDate.from("-271821-06-20").withCalendar("indian").add({months:-3}), RangeError, "indian longer partial minimum clamp");
shouldBe(Temporal.PlainDate.from("-271821-06-19").withCalendar("indian").add({months:-2}).withCalendar("iso8601").toString(), "-271821-04-19", "indian exact multi-month minimum boundary");
const indianMinimum = Temporal.PlainDate.from("-271821-04-19").withCalendar("indian");
const indianMinimumNextMonth = indianMinimum.add({months:1});
shouldBe(indianMinimum.until(indianMinimumNextMonth, {largestUnit:"months"}).toString(), "P1M", "indian minimum until progress");
shouldBe(indianMinimumNextMonth.until(indianMinimum, {largestUnit:"months"}).toString(), "-P1M", "indian minimum reverse until progress");

// Calendar-native arithmetic must enforce the exact partial-year ISO limits.
shouldBe(Temporal.PlainDate.from("+275760-08-13").withCalendar("indian").add({months:1}).withCalendar("iso8601").toString(), "+275760-09-13", "indian exact maximum boundary");
shouldThrow(() => Temporal.PlainDate.from("+275760-08-14").withCalendar("indian").add({months:1}), RangeError, "indian maximum boundary");
shouldThrow(() => Temporal.PlainDate.from("+275760-07-14").withCalendar("indian").add({months:2}), RangeError, "indian multi-month partial maximum clamp");
const copticMaximum = Temporal.PlainDate.from("+275760-09-13").withCalendar("coptic");
const copticPriorYear = copticMaximum.subtract({months:13});
shouldBe(copticPriorYear.add({months:13}).equals(copticMaximum), true, "coptic exact 13-month maximum boundary");
shouldBe(copticPriorYear.until(copticMaximum, {largestUnit:"months"}).toString(), "P13M", "coptic extreme 13-month difference");

// Final ISO day/week movement is checked after the native year/month baseline.
shouldThrow(() => Temporal.PlainDate.from("+275760-08-13").withCalendar("indian").add({months:1, days:1}), RangeError, "indian final day beyond maximum");
shouldThrow(() => Temporal.PlainDate.from("+275760-09-07").withCalendar("indian").add({weeks:1}), RangeError, "indian final week beyond maximum");

// Partial Temporal types use their own limits and may canonicalize an out-of-PlainDate-range reference date.
shouldBe(Temporal.PlainYearMonth.from({year:275682, monthCode:"M07", calendar:"indian"}).toString(), "+275760-09-23[u-ca=indian]", "indian maximum PlainYearMonth");
shouldBe(Temporal.PlainYearMonth.from({year:-272442, monthCode:"M01", calendar:"persian"}).toString(), "-271821-04-11[u-ca=persian]", "persian boundary PlainYearMonth");
shouldBe(Temporal.PlainMonthDay.from({year:-272442, monthCode:"M01", day:1, calendar:"persian"}).toString(), "1972-03-21[u-ca=persian]", "persian boundary PlainMonthDay");
shouldThrow(() => Temporal.PlainYearMonth.from("+275760-09-01[u-ca=indian]").add({months:1}), RangeError, "indian PlainYearMonth maximum boundary");

// Fixed-solar month differences must not walk millions of native months one at a time.
for (const [calendar, startFields, endFields, expectedMonths] of [
    ["indian", "-250000-01-01", "+250000-01-01", 6000000],
    ["persian", {year:-250000, month:1, day:15}, {year:250000, month:1, day:15}, 6000000],
    ["coptic", {year:-250000, month:1, day:15}, {year:250000, month:1, day:15}, 6500000],
]) {
    const start = typeof startFields === "string" ? Temporal.PlainDate.from(startFields).withCalendar(calendar) : Temporal.PlainDate.from({...startFields, calendar});
    const end = typeof endFields === "string" ? Temporal.PlainDate.from(endFields).withCalendar(calendar) : Temporal.PlainDate.from({...endFields, calendar});
    const forward = start.until(end, {largestUnit:"months"});
    const reverse = end.until(start, {largestUnit:"months"});
    shouldBe(forward.toString(), `P${expectedMonths}M`, `${calendar} wide forward month difference`);
    shouldBe(reverse.toString(), `-P${expectedMonths}M`, `${calendar} wide reverse month difference`);
    shouldBe(start.until(end, {largestUnit:"years"}).toString(), "P500000Y", `${calendar} wide year difference`);
}

// Extreme .add({years}) that fits int32 but drives ucal_add(UCAL_EXTENDED_YEAR) far enough that
// epochMs can exceed int64_t's range; must throw RangeError, not crash or produce a wrong date.
for (const calendar of ["chinese", "dangi", "hebrew"]) {
    const start = calendar === "hebrew"
        ? Temporal.PlainDate.from({year:5760, monthCode:"M01", day:1, calendar})
        : Temporal.PlainDate.from({year:2000, monthCode:"M01", day:1, calendar});
    shouldThrow(() => start.add({years: 2147483637}), RangeError, `${calendar} add extreme years (just under INT32_MAX)`);
    shouldThrow(() => start.add({years: 2147483647}), RangeError, `${calendar} add extreme years (exactly INT32_MAX)`);
    shouldThrow(() => start.subtract({years: 2147483637}), RangeError, `${calendar} subtract extreme years`);
}

// Hebrew has no extreme-year ISO-fallback (unlike chinese/dangi), so a huge year reaches
// construction directly.
shouldThrow(() => Temporal.PlainDate.from({year: 2147483637, monthCode: "M01", day: 1, calendar: "hebrew"}), RangeError, "hebrew extreme positive year construction");
shouldThrow(() => Temporal.PlainDate.from({year: -2147483637, monthCode: "M01", day: 1, calendar: "hebrew"}), RangeError, "hebrew extreme negative year construction");
