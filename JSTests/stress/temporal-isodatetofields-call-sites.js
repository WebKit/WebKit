//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (String(actual) !== String(expected))
        throw new Error(`${msg}: expected ${JSON.stringify(String(expected))} but got ${JSON.stringify(String(actual))}`);
}

function shouldThrow(fn, type, msg) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`${msg}: expected ${type.name} but got ${err}`);
}

const ym = (cal) => Temporal.PlainYearMonth.from({ year: 2021, month: 5, calendar: cal });

// --- AddDurationToYearMonth: ISODateToFields(~year-month~) at steps 9 and 13 ---
// The receiver's era/eraYear are NOT part of ISODateToFields' output; only monthCode and year are,
// so an era-bearing calendar must round-trip through the arithmetic year alone.
shouldBe(ym("iso8601").add({ months: 1 }), "2021-06", "iso add P1M");
shouldBe(ym("iso8601").add({ months: 13 }), "2022-06", "iso add P13M");
shouldBe(ym("iso8601").subtract({ months: 1 }), "2021-04", "iso sub P1M");

shouldBe(ym("japanese").add({ months: 1 }).toString(), "2021-06-01[u-ca=japanese]", "japanese add P1M");
shouldBe(ym("japanese").add({ months: 13 }).toString(), "2022-06-01[u-ca=japanese]", "japanese add P13M");
shouldBe(ym("japanese").add({ years: 1 }).toString(), "2022-05-01[u-ca=japanese]", "japanese add P1Y");
shouldBe(ym("japanese").subtract({ months: 1 }).toString(), "2021-04-01[u-ca=japanese]", "japanese sub P1M");
shouldBe(ym("gregory").add({ months: 1 }).toString(), "2021-06-01[u-ca=gregory]", "gregory add P1M");
shouldBe(ym("roc").add({ months: 1 }).toString(), "3932-06-01[u-ca=roc]", "roc add P1M");
shouldBe(ym("roc").add({ years: 1 }).toString(), "3933-05-01[u-ca=roc]", "roc add P1Y");

// Lunisolar: leap-month insertion means the ordinal month cannot be carried across years, so
// ISODateToFields must hand CalendarYearMonthFromFields a monthCode.
shouldBe(ym("hebrew").add({ months: 1 }).toString(), "-001739-02-06[u-ca=hebrew]", "hebrew add P1M");
shouldBe(ym("hebrew").add({ months: 13 }).toString(), "-001738-01-25[u-ca=hebrew]", "hebrew add P13M");
shouldBe(ym("hebrew").subtract({ months: 1 }).toString(), "-001740-12-09[u-ca=hebrew]", "hebrew sub P1M");
shouldBe(ym("chinese").add({ months: 1 }).toString(), "2021-07-10[u-ca=chinese]", "chinese add P1M");
shouldBe(ym("chinese").add({ months: 13 }).toString(), "2022-06-29[u-ca=chinese]", "chinese add P13M");
shouldBe(ym("chinese").subtract({ months: 1 }).toString(), "2021-05-12[u-ca=chinese]", "chinese sub P1M");

// An era-bearing receiver built FROM era+eraYear must add identically to one built from year.
shouldBe(Temporal.PlainYearMonth.from({ era: "reiwa", eraYear: 3, month: 5, calendar: "japanese" })
    .add({ months: 1 }).toString(), "2021-06-01[u-ca=japanese]", "japanese add P1M from era");

// ISO takes the pure isoDateAdd overload so the extreme boundary years do not get ICU-clamped.
shouldBe(Temporal.PlainYearMonth.from("+275760-08").add({ months: 1 }), "+275760-09", "add to max boundary");
shouldThrow(() => Temporal.PlainYearMonth.from("+275760-09").add({ months: 1 }), RangeError, "add past max");
shouldThrow(() => Temporal.PlainYearMonth.from("-271821-05").subtract({ months: 1 }), RangeError, "sub past min");

for (const [cal, year, isoPrefix] of [["buddhist", 2021, "1478"], ["roc", -433, "1478"], ["japanese", 1478, "1478"]]) {
    const base = Temporal.PlainYearMonth.from({ year, month: 5, calendar: cal });
    shouldBe(base.toString(), `${isoPrefix}-05-01[u-ca=${cal}]`, `${cal} base at ISO ${isoPrefix}`);
    shouldBe(base.add({ months: 0 }).month, 5, `${cal}: add P0M must be identity`);
    shouldBe(base.add({ months: 1 }).month, 6, `${cal}: add P1M`);
    shouldBe(base.subtract({ months: 1 }).month, 4, `${cal}: sub P1M`);
}
// Outside the affected ISO-year range the same calendars are correct, which pins the boundary.
shouldBe(Temporal.PlainYearMonth.from({ year: 2200, month: 5, calendar: "buddhist" })
    .add({ months: 0 }).toString(), "1657-05-01[u-ca=buddhist]", "buddhist add P0M above range");
shouldBe(Temporal.PlainYearMonth.from({ year: 543, month: 5, calendar: "buddhist" })
    .add({ months: 0 }).toString(), "0000-05-01[u-ca=buddhist]", "buddhist add P0M below range");
// gregory at the same ISO year is unaffected, isolating the proleptic-Gregorian era path.
shouldBe(Temporal.PlainYearMonth.from({ year: 1478, month: 5, calendar: "gregory" })
    .add({ months: 0 }).month, 5, "gregory add P0M at ISO 1478");

// --- PlainYearMonth.toPlainDate: ISODateToFields(~year-month~) then CalendarMergeFields(«day») ---
shouldBe(ym("iso8601").toPlainDate({ day: 15 }), "2021-05-15", "iso toPlainDate day=15");
shouldBe(ym("iso8601").toPlainDate({ day: 31 }), "2021-05-31", "iso toPlainDate day=31");
shouldBe(ym("japanese").toPlainDate({ day: 1 }).toString(), "2021-05-01[u-ca=japanese]", "japanese toPlainDate day=1");
shouldBe(ym("japanese").toPlainDate({ day: 31 }).toString(), "2021-05-31[u-ca=japanese]", "japanese toPlainDate day=31");
shouldBe(ym("hebrew").toPlainDate({ day: 1 }).toString(), "-001739-01-07[u-ca=hebrew]", "hebrew toPlainDate day=1");
shouldBe(ym("hebrew").toPlainDate({ day: 31 }).toString(), "-001739-02-05[u-ca=hebrew]", "hebrew toPlainDate day=31");
shouldBe(ym("chinese").toPlainDate({ day: 15 }).toString(), "2021-06-24[u-ca=chinese]", "chinese toPlainDate day=15");
shouldBe(ym("chinese").toPlainDate({ day: 31 }).toString(), "2021-07-09[u-ca=chinese]", "chinese toPlainDate day=31");

// --- ToTemporalYearMonth step 12 / PlainDate.toPlainYearMonth ---
for (const [cal, expected] of [["japanese", "2021-05-01[u-ca=japanese]"], ["hebrew", "2021-05-12[u-ca=hebrew]"],
    ["chinese", "2021-05-12[u-ca=chinese]"]]) {
    shouldBe(Temporal.PlainYearMonth.from(`2021-05-17[u-ca=${cal}]`).toString(), expected, `${cal} YM.from string`);
    shouldBe(Temporal.PlainDate.from(`2021-05-17[u-ca=${cal}]`).toPlainYearMonth().toString(), expected, `${cal} PD.toPlainYearMonth`);
}
shouldBe(Temporal.PlainYearMonth.from("2021-05-17"), "2021-05", "iso YM.from string");
// The reference day is canonical (1 for ISO), which is why step 14 forces ~constrain~.
shouldBe(Temporal.PlainYearMonth.from("2020-05-23[u-ca=chinese]").toString(), "2020-05-23[u-ca=chinese]", "chinese YM.from leap-month year");
shouldBe(Temporal.PlainYearMonth.from("2022-03-05[u-ca=hebrew]").toString(), "2022-03-04[u-ca=hebrew]", "hebrew YM.from leap-month year");

// --- ToTemporalMonthDay step 13 / PlainDate.toPlainMonthDay ---
shouldBe(Temporal.PlainMonthDay.from("2021-05-17"), "05-17", "iso MD.from string");
shouldBe(Temporal.PlainMonthDay.from("2024-02-29"), "02-29", "iso MD.from Feb 29");
for (const [cal, expected] of [["japanese", "1972-05-17[u-ca=japanese]"], ["hebrew", "1972-05-19[u-ca=hebrew]"],
    ["chinese", "1972-05-18[u-ca=chinese]"]]) {
    shouldBe(Temporal.PlainMonthDay.from(`2021-05-17[u-ca=${cal}]`).toString(), expected, `${cal} MD.from string`);
    shouldBe(Temporal.PlainDate.from(`2021-05-17[u-ca=${cal}]`).toPlainMonthDay().toString(), expected, `${cal} PD.toPlainMonthDay`);
}

// --- getter/resolution agreement ---
for (const cal of ["iso8601", "gregory", "hebrew", "chinese", "dangi", "islamic-civil",
    "islamic-tbla", "islamic-umalqura", "japanese", "buddhist", "roc", "coptic", "ethiopic",
    "ethioaa", "persian", "indian"]) {
    for (const iso of ["2020-05-01", "2020-06-21", "1978-02-28", "2024-02-29", "2023-06-18"]) {
        const d = Temporal.PlainDate.from(iso).withCalendar(cal);
        const tag = `${cal} ${iso}`;
        shouldBe(d.with({ day: d.day }).equals(d), true, `${tag}: with({day: d.day}) is identity`);
        shouldBe(d.with({ year: d.year }).equals(d), true, `${tag}: with({year: d.year}) is identity`);
        shouldBe(d.with({ monthCode: d.monthCode }).equals(d), true, `${tag}: with({monthCode}) is identity`);
        shouldBe(d.with({ month: d.month }).equals(d), true, `${tag}: with({month: d.month}) is identity`);
        if (d.era !== undefined)
            shouldBe(d.with({ era: d.era, eraYear: d.eraYear }).equals(d), true, `${tag}: with({era, eraYear}) is identity`);
    }
}
