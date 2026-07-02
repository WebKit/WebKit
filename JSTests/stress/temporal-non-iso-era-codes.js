//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}

// Per icu4x components/calendar/src/cal/{coptic,ethiopian,indian,...}.rs.
const expectedEras = [
    ["coptic", "am"],
    ["ethiopic", "am"],
    ["ethioaa", "aa"],
    ["indian", "shaka"],
    ["persian", "ap"],
    ["roc", "roc"],
    ["hebrew", "am"],
    ["buddhist", "be"],
    ["gregory", "ce"],
    ["islamic-umalqura", "ah"],
    ["islamic-civil", "ah"],
    ["islamic-tbla", "ah"],
];

for (const [cal, expected] of expectedEras) {
    const d = new Temporal.PlainDate(2024, 1, 15, cal);
    shouldBe(d.era, expected, `${cal}.era`);
}

// Japanese era depends on the date; spot-check the current Reiwa era.
shouldBe(new Temporal.PlainDate(2024, 1, 15, "japanese").era, "reiwa", "japanese (2024).era");
shouldBe(new Temporal.PlainDate(1990, 1, 15, "japanese").era, "heisei", "japanese (1990).era");
shouldBe(new Temporal.PlainDate(1980, 1, 15, "japanese").era, "showa", "japanese (1980).era");

// ISO calendar must not have an era.
shouldBe(new Temporal.PlainDate(2024, 1, 15).era, undefined, "iso8601.era");

// Reverse direction: PlainDate.from() with era field must accept the canonical era code.
const fromCoptic = Temporal.PlainDate.from({ era: "am", eraYear: 1740, month: 9, day: 1, calendar: "coptic" });
shouldBe(fromCoptic.era, "am", "from({era:'am'}) coptic round-trip");

const fromIndian = Temporal.PlainDate.from({ era: "shaka", eraYear: 1945, month: 1, day: 15, calendar: "indian" });
shouldBe(fromIndian.era, "shaka", "from({era:'shaka'}) indian round-trip");

// Wrong era code must throw.
let threw = false;
try {
    Temporal.PlainDate.from({ era: "ce", eraYear: 1740, month: 9, day: 1, calendar: "coptic" });
} catch (e) {
    threw = e instanceof RangeError;
}
if (!threw)
    throw new Error("coptic.from with era='ce' should throw RangeError");

threw = false;
try {
    Temporal.PlainDate.from({ era: "saka", eraYear: 1945, month: 1, day: 15, calendar: "indian" });
} catch (e) {
    threw = e instanceof RangeError;
}
if (!threw)
    throw new Error("indian.from with era='saka' should throw RangeError");
