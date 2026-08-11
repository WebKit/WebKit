//@ requireOptions("--useTemporal=1")
// Formatting a Temporal value must preserve the requested era width
// (era: "long" / "short" / "narrow"), matching an equivalent legacy Date.
// GetDateTimeFormat used to copy only a single 'G' into the format options,
// collapsing era:"long" (GGGG) and era:"narrow" (GGGGG) to the short form.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

const plainDate = new Temporal.PlainDate(2026, 6, 2);
const plainDateTime = new Temporal.PlainDateTime(2026, 6, 2, 10, 30, 45);
const date = new Date(Date.UTC(2026, 5, 2, 10, 30, 45));

const optionsList = [
    { era: "long", year: "numeric", month: "long", day: "numeric" },
    { era: "short", year: "numeric", month: "long", day: "numeric" },
    { era: "narrow", year: "numeric", month: "long", day: "numeric" },
    { era: "long", year: "numeric", month: "numeric", day: "numeric" },
];

const locales = ["ja", "zh", "ko", "en-US", "de"];

for (const locale of locales) {
    for (const options of optionsList) {
        const dtf = new Intl.DateTimeFormat(locale, { ...options, timeZone: "UTC" });
        const label = `${locale} ${JSON.stringify(options)}`;

        // All requested fields are date fields, so PlainDate and PlainDateTime
        // must format exactly like a legacy Date at the same calendar date.
        shouldBe(dtf.format(plainDate), dtf.format(date), `${label} format(PlainDate)`);
        shouldBe(dtf.format(plainDateTime), dtf.format(date), `${label} format(PlainDateTime)`);

        const dateParts = JSON.stringify(dtf.formatToParts(date));
        shouldBe(JSON.stringify(dtf.formatToParts(plainDate)), dateParts, `${label} formatToParts(PlainDate)`);

        const laterPlainDate = new Temporal.PlainDate(2026, 7, 4);
        const laterDate = new Date(Date.UTC(2026, 6, 4));
        shouldBe(dtf.formatRange(plainDate, laterPlainDate), dtf.formatRange(date, laterDate), `${label} formatRange(PlainDate)`);
    }
}

// era: "long" must not degrade to the short form ("AD") in ko.
{
    const dtf = new Intl.DateTimeFormat("ko", { era: "long", year: "numeric", month: "long", day: "numeric", timeZone: "UTC" });
    const formatted = dtf.format(plainDate);
    shouldBe(formatted.includes("서기"), true, `ko era long PlainDate contains 서기: ${formatted}`);
}
