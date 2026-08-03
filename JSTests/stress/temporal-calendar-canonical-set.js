//@ requireOptions("--useTemporal=1")

// Regression for proposal-intl-era-monthcode Phase 0.

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

const canonical = ["buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
    "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
    "islamic-umalqura", "iso8601", "japanese", "persian", "roc"];
const nonCanonical = ["islamic", "islamic-rgsa", "bangla", "gujarati", "kannada",
    "marathi", "odia", "tamil", "telugu", "vikram"];

// Intl.supportedValuesOf contains the canonical set. Under the flag, JSC also
// excludes non-canonical entries (V8 currently still lists them).
{
    const ids = Intl.supportedValuesOf("calendar");
    for (const c of canonical)
        shouldBe(ids.includes(c), true, `supportedValuesOf contains ${c}`);
    if (typeof $vm !== "undefined") {
        for (const nc of nonCanonical)
            shouldBe(ids.includes(nc), false, `supportedValuesOf must not contain ${nc}`);
    }
}

// Temporal accepts canonical.
for (const id of canonical) {
    const pd = Temporal.PlainDate.from({ year: 2024, month: 1, day: 1, calendar: id });
    shouldBe(pd.calendarId, id, `Temporal accepts ${id}`);
}

// Legacy CLDR aliases canonicalize.
shouldBe(Temporal.PlainDate.from({ year: 1445, month: 1, day: 1, calendar: "islamicc" }).calendarId, "islamic-civil", "islamicc -> islamic-civil");
shouldBe(Temporal.PlainDate.from({ year: 2016, month: 1, day: 1, calendar: "ethiopic-amete-alem" }).calendarId, "ethioaa", "ethiopic-amete-alem -> ethioaa");

// Temporal rejects non-canonical.
for (const id of [...nonCanonical, "nonexistent"]) {
    let threw = false;
    try { Temporal.PlainDate.from({ year: 2024, month: 1, day: 1, calendar: id }); }
    catch (e) { threw = e instanceof RangeError; }
    shouldBe(threw, true, `Temporal rejects ${id}`);
}

// Intl.DateTimeFormat: bare islamic -> islamic-tbla (JSC-only; V8 hasn't caught up).
if (typeof $vm !== "undefined") {
    shouldBe(new Intl.DateTimeFormat("en", { calendar: "islamic" }).resolvedOptions().calendar, "islamic-tbla", "islamic -> islamic-tbla");
}

// Indic variants fall back to a member of AvailableCalendars.
{
    const available = Intl.supportedValuesOf("calendar");
    for (const id of ["bangla", "gujarati", "kannada", "marathi", "odia", "tamil", "telugu", "vikram"]) {
        const resolved = new Intl.DateTimeFormat("en", { calendar: id }).resolvedOptions().calendar;
        shouldBe(available.includes(resolved), true, `${id} falls back into AvailableCalendars: got ${resolved}`);
    }
}
