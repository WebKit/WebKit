//@ requireOptions("--useTemporal=1")
// Temporal formatter pattern generation must request the calendar from ICU in BCP47
// form via the -u-ca- extension (e.g. "ar-SA-u-ca-gregory"). Passing a BCP47 calendar
// value through ICU's legacy @calendar= keyword (e.g. "ar-SA@calendar=gregory") is
// ignored by ICU when the BCP47 name differs from the legacy name ("gregory" vs
// "gregorian", "ethioaa" vs "ethiopic-amete-alem"), so pattern selection silently
// falls back to the locale's default calendar. For ar-SA (default: islamic-umalqura)
// that injects a spurious era field and long month names into gregorian output.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function partTypes(parts) {
    return parts.map((part) => part.type).join(",");
}

function hasEra(parts) {
    return parts.some((part) => part.type === "era");
}

const plainDate = new Temporal.PlainDate(2024, 1, 15);
const plainDateTime = new Temporal.PlainDateTime(2024, 1, 15, 10, 30, 45);
const instant = new Temporal.Instant(BigInt(Date.UTC(2024, 0, 15, 10, 30, 45)) * 1_000_000n);
const date = new Date(Date.UTC(2024, 0, 15));
const dateTime = new Date(Date.UTC(2024, 0, 15, 10, 30, 45));

// 1. GetDateTimeFormat path with default fields.
//    ar-SA's default calendar is islamic-umalqura. With ca=gregory, Temporal values
//    must be formatted with the gregorian pattern — identical to Date formatting.
{
    const fmt = new Intl.DateTimeFormat("ar-SA-u-ca-gregory", { timeZone: "UTC" });
    shouldBe(fmt.resolvedOptions().calendar, "gregory", "resolved calendar");

    shouldBe(hasEra(fmt.formatToParts(plainDate)), false,
        "ar-SA-u-ca-gregory default fields: PlainDate must not include era");
    shouldBe(partTypes(fmt.formatToParts(plainDate)), partTypes(fmt.formatToParts(date)),
        "ar-SA-u-ca-gregory default fields: PlainDate parts vs Date parts");
    shouldBe(fmt.format(plainDate), fmt.format(date),
        "ar-SA-u-ca-gregory default fields: PlainDate vs Date");
}

// 2. GetDateTimeFormat path with explicit fields.
{
    const options = { year: "numeric", month: "numeric", day: "numeric", timeZone: "UTC" };
    const fmt = new Intl.DateTimeFormat("ar-SA-u-ca-gregory", options);

    shouldBe(hasEra(fmt.formatToParts(plainDate)), false,
        "ar-SA-u-ca-gregory explicit fields: PlainDate must not include era");
    shouldBe(partTypes(fmt.formatToParts(plainDate)), partTypes(fmt.formatToParts(date)),
        "ar-SA-u-ca-gregory explicit fields: PlainDate parts vs Date parts");
    shouldBe(fmt.format(plainDate), fmt.format(date),
        "ar-SA-u-ca-gregory explicit fields: PlainDate vs Date");
}

// 3. GetDateTimeFormat path for other Temporal kinds.
{
    const fmt = new Intl.DateTimeFormat("ar-SA-u-ca-gregory", {
        year: "numeric", month: "numeric", day: "numeric",
        hour: "numeric", minute: "2-digit", second: "2-digit",
        timeZone: "UTC",
    });

    shouldBe(hasEra(fmt.formatToParts(plainDateTime)), false,
        "ar-SA-u-ca-gregory: PlainDateTime must not include era");
    shouldBe(fmt.format(plainDateTime), fmt.format(dateTime),
        "ar-SA-u-ca-gregory: PlainDateTime vs Date");

    shouldBe(hasEra(fmt.formatToParts(instant)), false,
        "ar-SA-u-ca-gregory: Instant must not include era");
    shouldBe(fmt.format(instant), fmt.format(dateTime),
        "ar-SA-u-ca-gregory: Instant vs Date");
}

// 4. AdjustDateTimeStyleFormat path: dateStyle+timeStyle formatting a PlainDate has
//    conflicting (time) fields, forcing a date-only pattern to be regenerated.
{
    for (const dateStyle of ["full", "long", "medium", "short"]) {
        const fmt = new Intl.DateTimeFormat("ar-SA-u-ca-gregory",
            { dateStyle, timeStyle: "short", timeZone: "UTC" });
        shouldBe(hasEra(fmt.formatToParts(plainDate)), false,
            `ar-SA-u-ca-gregory dateStyle=${dateStyle}+timeStyle: PlainDate must not include era`);
    }
}

// 5. ethioaa is the other calendar whose BCP47 name differs from the ICU legacy name
//    ("ethiopic-amete-alem"). Temporal and Date formatting must agree.
{
    const fmt = new Intl.DateTimeFormat("en-u-ca-ethioaa", { timeZone: "UTC" });
    shouldBe(fmt.resolvedOptions().calendar, "ethioaa", "resolved calendar");
    shouldBe(partTypes(fmt.formatToParts(plainDate)), partTypes(fmt.formatToParts(date)),
        "en-u-ca-ethioaa: PlainDate parts vs Date parts");
    shouldBe(fmt.format(plainDate), fmt.format(date),
        "en-u-ca-ethioaa: PlainDate vs Date");
}

// 6. Calendars whose BCP47 and ICU legacy names coincide must keep working.
{
    const fmt = new Intl.DateTimeFormat("en-u-ca-islamic-umalqura", { timeZone: "UTC" });
    shouldBe(fmt.format(plainDate), fmt.format(date),
        "en-u-ca-islamic-umalqura: PlainDate vs Date");
    shouldBe(hasEra(fmt.formatToParts(plainDate)), true,
        "en-u-ca-islamic-umalqura: PlainDate must include era");
}
