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

// gregory / roc / islamic-civil / ethiopic: non-positive eraYear remaps to opposite era.
{
    const d = Temporal.PlainDate.from({ calendar: "gregory", era: "ce", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d, 0, 1, 1, "bce", 1, "gregory ce 0 -> bce 1");
    const d2 = Temporal.PlainDate.from({ calendar: "gregory", era: "bce", eraYear: -1, monthCode: "M01", day: 1 });
    assertPD(d2, 2, 1, 1, "ce", 2, "gregory bce -1 -> ce 2");
    const d3 = Temporal.PlainDate.from({ calendar: "roc", era: "roc", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d3, 0, 1, 1, "broc", 1, "roc roc 0 -> broc 1");
    const d4 = Temporal.PlainDate.from({ calendar: "islamic-civil", era: "ah", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d4, 0, 1, 1, "bh", 1, "islamic-civil ah 0 -> bh 1");
    const d5 = Temporal.PlainDate.from({ calendar: "ethiopic", era: "am", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d5, 0, 1, 1, "aa", 5500, "ethiopic am 0 -> aa 5500");
    const d6 = Temporal.PlainDate.from({ calendar: "ethiopic", era: "aa", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d6, -5500, 1, 1, "aa", 0, "ethiopic aa 0 (not remapped)");
}

// Single-era calendars: negative eraYear NOT remapped.
for (const [cal, era] of [["buddhist","be"], ["coptic","am"], ["ethioaa","aa"], ["hebrew","am"], ["indian","shaka"], ["persian","ap"]]) {
    for (const y of [-1, 0, 1]) {
        const d = Temporal.PlainDate.from({ calendar: cal, era, eraYear: y, monthCode: "M01", day: 1 });
        shouldBe(d.era, era, `${cal} ${era} ${y} era`);
        shouldBe(d.eraYear, y, `${cal} ${era} ${y} eraYear`);
    }
}

// Japanese dated era boundaries + pre-Gregorian meiji fallback.
{
    const d = Temporal.PlainDate.from({ calendar: "japanese", era: "reiwa", eraYear: 1, monthCode: "M04", day: 30 });
    assertPD(d, 2019, 4, 30, "heisei", 31, "reiwa 1 before start -> heisei 31");
    const d2 = Temporal.PlainDate.from({ calendar: "japanese", era: "heisei", eraYear: 31, monthCode: "M05", day: 1 });
    assertPD(d2, 2019, 5, 1, "reiwa", 1, "heisei 31 on reiwa start -> reiwa 1");
    const d3 = Temporal.PlainDate.from({ calendar: "japanese", era: "meiji", eraYear: 5, monthCode: "M12", day: 31 });
    assertPD(d3, 1872, 12, 31, "ce", 1872, "meiji 5 (pre-1873) -> ce 1872");
    const d4 = Temporal.PlainDate.from({ calendar: "japanese", era: "ce", eraYear: 1873, monthCode: "M01", day: 1 });
    assertPD(d4, 1873, 1, 1, "meiji", 6, "ce 1873 -> meiji 6");
}

// Case-insensitive alias canonicalizes then remaps.
{
    const d = Temporal.PlainDate.from({ calendar: "gregory", era: "AD", eraYear: 0, monthCode: "M01", day: 1 });
    assertPD(d, 0, 1, 1, "bce", 1, "AD 0 -> bce 1");
}

