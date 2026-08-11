//@ requireOptions("--useTemporal=1")

// The Japanese calendar is Gregorian-based: its year/month/day/monthCode (and
// daysInMonth/daysInYear/monthsInYear/inLeapYear) are the proleptic-Gregorian
// (i.e. ISO) values for every year. Only era/eraYear are era-relative.
//
// These accessors must NOT route through ICU's Japanese calendar, which uses
// Julian arithmetic before the 1582 cutover and would give a different month
// or day for old dates. They route through the proleptic Gregorian calendar,
// so the values always match the plain ISO date. Only era/eraYear gate on the
// 1873 (Meiji 6) transition, where Temporal reports "ce"/"bce" for Meiji 1-5.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function monthCode(month) {
    return "M" + (month < 10 ? "0" : "") + month;
}

// Japanese date fields equal the ISO date fields (any year), and era/eraYear match.
function checkDateFields(isoYear, isoMonth, isoDay, expectedEra, expectedEraYear) {
    const jp = new Temporal.PlainDate(isoYear, isoMonth, isoDay, "japanese");
    const iso = new Temporal.PlainDate(isoYear, isoMonth, isoDay); // iso8601
    const tag = `${isoYear}-${isoMonth}-${isoDay}`;

    // Date fields equal the ISO values.
    shouldBe(jp.year, isoYear, `${tag} year`);
    shouldBe(jp.month, isoMonth, `${tag} month`);
    shouldBe(jp.monthCode, monthCode(isoMonth), `${tag} monthCode`);
    shouldBe(jp.day, isoDay, `${tag} day`);

    // ... and match the iso8601 date exactly, including the derived quantities.
    shouldBe(jp.month, iso.month, `${tag} month == iso`);
    shouldBe(jp.monthCode, iso.monthCode, `${tag} monthCode == iso`);
    shouldBe(jp.day, iso.day, `${tag} day == iso`);
    shouldBe(jp.daysInMonth, iso.daysInMonth, `${tag} daysInMonth == iso`);
    shouldBe(jp.daysInYear, iso.daysInYear, `${tag} daysInYear == iso`);
    shouldBe(jp.monthsInYear, 12, `${tag} monthsInYear`);
    shouldBe(jp.inLeapYear, iso.inLeapYear, `${tag} inLeapYear == iso`);

    // Era / eraYear.
    shouldBe(jp.era, expectedEra, `${tag} era`);
    shouldBe(jp.eraYear, expectedEraYear, `${tag} eraYear`);
}

// Pre-1582 Julian territory in ICU: fields must be proleptic Gregorian, not Julian.
checkDateFields(1000, 6, 15, "ce", 1000);
checkDateFields(1300, 2, 28, "ce", 1300); // 1300: Julian leap, proleptic-Gregorian common -> Feb has 28 days
checkDateFields(1500, 2, 28, "ce", 1500); // 1500: same divergence
checkDateFields(1500, 3, 1, "ce", 1500);
checkDateFields(1582, 10, 4, "ce", 1582); // day before the historical Julian->Gregorian cutover
checkDateFields(1582, 10, 15, "ce", 1582); // first historical Gregorian day; nothing special proleptically

// Between the 1582 cutover and Japan's 1873 Gregorian adoption.
checkDateFields(1700, 2, 28, "ce", 1700); // 1700: not a proleptic-Gregorian leap year
checkDateFields(1868, 1, 1, "ce", 1868); // Meiji began 1868-10-23, but Meiji 1-5 are reported as CE
checkDateFields(1868, 11, 1, "ce", 1868); // ICU would call this Meiji 1; Temporal reports CE
checkDateFields(1872, 12, 31, "ce", 1872); // Meiji 5 -> CE (last day before Gregorian adoption)

// Meiji 6 (1873) onward: real Japanese eras.
checkDateFields(1873, 1, 1, "meiji", 6); // Gregorian adoption; first day reported as "meiji"
checkDateFields(1900, 1, 1, "meiji", 33);
checkDateFields(1920, 1, 1, "taisho", 9);
checkDateFields(1930, 1, 1, "showa", 5);
checkDateFields(2000, 2, 29, "heisei", 12); // 2000 is a proleptic-Gregorian leap year
checkDateFields(2024, 6, 1, "reiwa", 6);

// BCE / year <= 0 (proleptic).
checkDateFields(0, 1, 1, "bce", 1); // ISO year 0 == 1 BCE
checkDateFields(-100, 7, 4, "bce", 101);

// Era boundary at 1873-01-01 (Meiji 6) and the modern era transitions.
// This is the one place the 1873 constant is load-bearing: ICU4C reports "meiji"
// for 1868-1872, but Temporal treats Meiji 1-5 as CE. The transition endpoints
// below are fixed by CLDR/ICU era data.
function checkEra(isoY, isoM, isoD, era, eraYear) {
    const pd = new Temporal.PlainDate(isoY, isoM, isoD, "japanese");
    const tag = `${isoY}-${isoM}-${isoD}`;
    shouldBe(pd.era, era, `${tag} era`);
    shouldBe(pd.eraYear, eraYear, `${tag} eraYear`);
    shouldBe(pd.year, isoY, `${tag} year`);
}

checkEra(1872, 12, 31, "ce", 1872); // last CE day
checkEra(1873, 1, 1, "meiji", 6); // first meiji day
checkEra(1912, 7, 29, "meiji", 45); // Meiji ends 1912-07-30
checkEra(1912, 7, 30, "taisho", 1); // Taisho begins
checkEra(1926, 12, 24, "taisho", 15); // Taisho ends 1926-12-25
checkEra(1926, 12, 25, "showa", 1); // Showa begins
checkEra(1989, 1, 7, "showa", 64); // Showa ends 1989-01-07
checkEra(1989, 1, 8, "heisei", 1); // Heisei begins
checkEra(2019, 4, 30, "heisei", 31); // Heisei ends 2019-04-30
checkEra(2019, 5, 1, "reiwa", 1); // Reiwa begins

// isoToCalendarFields batch path (used by .with / .from and by ZonedDateTime).
// A pre-cutover date must resolve proleptic-Gregorian month/monthCode/day so that
// .with() (which reuses the calendar-native fallback fields) lands on the right date.
{
    const pd = new Temporal.PlainDate(1500, 3, 10, "japanese");
    const changed = pd.with({ day: 20 });
    shouldBe(changed.year, 1500, "PlainDate.with year");
    shouldBe(changed.month, 3, "PlainDate.with month");
    shouldBe(changed.monthCode, "M03", "PlainDate.with monthCode");
    shouldBe(changed.day, 20, "PlainDate.with day");
    shouldBe(changed.era, "ce", "PlainDate.with era");
    shouldBe(changed.eraYear, 1500, "PlainDate.with eraYear");
}
{
    // Meiji-5 date: CE era, ISO fields, preserved across .with().
    const pd = new Temporal.PlainDate(1872, 6, 1, "japanese");
    shouldBe(pd.era, "ce", "1872 era");
    shouldBe(pd.eraYear, 1872, "1872 eraYear");
    const changed = pd.with({ day: 15 });
    shouldBe(changed.toString(), "1872-06-15[u-ca=japanese]", "1872 with day toString");
    shouldBe(changed.era, "ce", "1872 with era");
    shouldBe(changed.eraYear, 1872, "1872 with eraYear");
}
{
    // ZonedDateTime.with uses isoToCalendarFields for its fallback fields.
    const zdt = Temporal.ZonedDateTime.from("1500-03-10T12:00[UTC][u-ca=japanese]");
    shouldBe(zdt.era, "ce", "zdt era");
    shouldBe(zdt.eraYear, 1500, "zdt eraYear");
    shouldBe(zdt.month, 3, "zdt month");
    shouldBe(zdt.day, 10, "zdt day");
    const changed = zdt.with({ day: 20 });
    shouldBe(changed.month, 3, "zdt with month");
    shouldBe(changed.day, 20, "zdt with day");
    shouldBe(changed.era, "ce", "zdt with era");
    shouldBe(changed.eraYear, 1500, "zdt with eraYear");
}
