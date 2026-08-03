//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function assertPD(d, year, month, day, era, eraYear, label) {
    shouldBe(d.year, year, `${label} year`);
    shouldBe(d.month, month, `${label} month`);
    shouldBe(d.day, day, `${label} day`);
    shouldBe(d.era, era, `${label} era`);
    shouldBe(d.eraYear, eraYear, `${label} eraYear`);
}

// Non-Gregorian-structured calendars: add-year preserves calendar month/day (not ISO month/day).
{
    // Coptic M02 always has 30 days; adding 1 year to M02 D29 stays D29.
    const d = Temporal.PlainDate.from({ year: 1747, monthCode: "M02", day: 29, calendar: "coptic" });
    assertPD(d.add(new Temporal.Duration(1)), 1748, 2, 29, "am", 1748, "coptic add 1y preserves M02 D29");
    // Ethiopic same shape (13 months).
    const e = Temporal.PlainDate.from({ year: 2016, monthCode: "M13", day: 5, calendar: "ethiopic" });
    assertPD(e.add(new Temporal.Duration(1)), 2017, 13, 5, "am", 2017, "ethiopic add 1y preserves M13 D5");
}

// Islamic-civil add years/months preserves calendar frame.
{
    const d = Temporal.PlainDate.from({ year: 1443, monthCode: "M02", day: 29, calendar: "islamic-civil" });
    const added = d.add(new Temporal.Duration(0, 1));
    shouldBe(added.year, 1443, "islamic-civil add 1 month year");
    shouldBe(added.month, 3, "islamic-civil add 1 month month");
}

// Persian: 30-day month (Mordad) + 1 year stays 30-day.
{
    const d = Temporal.PlainDate.from({ year: 1400, monthCode: "M05", day: 30, calendar: "persian" });
    assertPD(d.add(new Temporal.Duration(1)), 1401, 5, 30, "ap", 1401, "persian add 1y preserves M05 D30");
}

// Gregorian-structured calendars (buddhist/roc/japanese) treat calendar year proleptically.
{
    // Buddhist BE 2125 M10 D04 = ISO 1582-10-04 (proleptic Gregorian, not Julian).
    const d = Temporal.PlainDate.from({ year: 2125, monthCode: "M10", day: 4, calendar: "buddhist" });
    assertPD(d.add(new Temporal.Duration(0, 0, 0, 3)), 2125, 10, 7, "be", 2125, "buddhist proleptic add 3 days");
    // ROC year=-329 (BROC 330) = ISO 1582; same +3 days = day 7.
    const r = Temporal.PlainDate.from({ year: -329, monthCode: "M10", day: 4, calendar: "roc" });
    assertPD(r.add(new Temporal.Duration(0, 0, 0, 3)), -329, 10, 7, "broc", 330, "roc proleptic add 3 days");
    // Japanese year=1582 = ISO 1582.
    const j = Temporal.PlainDate.from({ year: 1582, monthCode: "M10", day: 4, calendar: "japanese" });
    assertPD(j.add(new Temporal.Duration(0, 0, 0, 3)), 1582, 10, 7, "ce", 1582, "japanese proleptic add 3 days");
}

// until in non-Gregorian-structured calendar produces calendar-frame duration.
{
    const a = Temporal.PlainDate.from({ year: 1747, monthCode: "M02", day: 1, calendar: "coptic" });
    const b = Temporal.PlainDate.from({ year: 1748, monthCode: "M02", day: 1, calendar: "coptic" });
    const dur = a.until(b, { largestUnit: "years" });
    shouldBe(dur.years, 1, "coptic until: years");
    shouldBe(dur.months, 0, "coptic until: months");
    shouldBe(dur.days, 0, "coptic until: days");
}
