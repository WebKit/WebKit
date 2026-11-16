function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

// Locales chosen so era placement and trailing-space behaviour both vary.
const locales = ["en-US", "ja-JP", "zh-CN", "ar-EG", "he-IL", "de-DE", "fi-FI", "th-TH"];

const shapes = [
    { era: "short" },
    { era: "long" },
    { era: "narrow" },
    { era: "short", year: "numeric" },
    { era: "short", year: "numeric", month: "long", day: "numeric" },
    { era: "long", year: "2-digit", month: "2-digit", day: "2-digit" },
    { era: "short", year: "numeric", month: "numeric", day: "numeric", weekday: "long" },
    // Ends in dayPeriod for en-US, so ICU leaves no trailing space: the broken case.
    { era: "short", year: "numeric", hour: "numeric", minute: "numeric" },
];

// Dates before each calendar's era epoch, so the override fires.
const cases = [
    ["coptic", Date.UTC(100, 0, 1)],
    ["coptic", Date.UTC(283, 7, 1)],
    ["islamic-civil", Date.UTC(300, 0, 1)],
    ["islamic-umalqura", Date.UTC(500, 0, 1)],
    ["islamic-tbla", Date.UTC(300, 0, 1)],
    // Controls: era present natively, and a calendar with no override at all.
    ["coptic", Date.UTC(2020, 0, 1)],
    ["gregory", Date.UTC(100, 0, 1)],
];

let checked = 0;
let sawOverride = false;
for (const locale of locales) {
    for (const [calendar, epochMs] of cases) {
        for (const shape of shapes) {
            const format = new Intl.DateTimeFormat(locale, { calendar, timeZone: "UTC", ...shape });
            const formatted = format.format(epochMs);
            const parts = format.formatToParts(epochMs);
            const joined = parts.map(part => part.value).join("");
            expect(`${locale} ${calendar} ${JSON.stringify(shape)}`, joined, formatted);

            // A double-synthesized separator needs no separate check: it would make joined
            // longer than format(), which the assertion above already catches.
            for (const part of parts)
                expect(`${locale} ${calendar} empty ${part.type} part`, part.value.length > 0, true);

            if (parts.some(part => part.type === "era") && format.resolvedOptions().calendar === calendar)
                sawOverride = true;
            ++checked;
        }
    }
}

expect("cases checked", checked, locales.length * cases.length * shapes.length);
// Keeps the test from passing vacuously if era parts stop being produced.
expect("saw at least one era part", sawOverride, true);
