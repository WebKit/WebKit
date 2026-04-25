//@ skip if $hostOS == "windows" || $hostOS == "linux"
//@ requireOptions("--useTemporal=1")
// https://bugs.webkit.org/show_bug.cgi?id=310866
// Per spec, Intl.DateTimeFormat.resolvedOptions().timeZone returns the
// [[Identifier]] (case-normalized accepted form) — not [[PrimaryIdentifier]].
// The canonicalization to the IANA primary still happens internally for ICU
// formatting (so legacy aliases produce the same formatted output as their
// primary), and it is observable through Temporal.TimeZone.id.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldBeTrue(actual) { shouldBe(actual, true); }
function shouldBeFalse(actual) { shouldBe(actual, false); }

function resolved(tz) {
    return new Intl.DateTimeFormat("en", { timeZone: tz }).resolvedOptions().timeZone;
}

// resolvedOptions().timeZone preserves the accepted identifier (case-normalized)
// rather than collapsing onto the IANA primary.
shouldBe(resolved("Asia/Calcutta"), "Asia/Calcutta");
shouldBe(resolved("asia/calcutta"), "Asia/Calcutta");
shouldBe(resolved("Asia/Kolkata"), "Asia/Kolkata");

shouldBe(resolved("America/Buenos_Aires"), "America/Buenos_Aires");
shouldBe(resolved("America/Argentina/Buenos_Aires"), "America/Argentina/Buenos_Aires");

shouldBe(resolved("Europe/Kiev"), "Europe/Kiev");
shouldBe(resolved("Europe/Kyiv"), "Europe/Kyiv");

// UTC-equivalent names are likewise preserved as the accepted identifier.
shouldBe(resolved("UTC"), "UTC");
shouldBe(resolved("Etc/UTC"), "Etc/UTC");
shouldBe(resolved("GMT"), "GMT");
shouldBe(resolved("Etc/GMT"), "Etc/GMT");
shouldBe(resolved("Universal"), "Universal");
shouldBe(resolved("Zulu"), "Zulu");
shouldBe(resolved("Greenwich"), "Greenwich");

// Intl.supportedValuesOf("timeZone") exposes IANA primary identifiers, not the
// older CLDR canonical aliases.
const all = Intl.supportedValuesOf("timeZone");
shouldBeTrue(all.includes("Asia/Kolkata"));
shouldBeFalse(all.includes("Asia/Calcutta"));
shouldBeTrue(all.includes("Europe/Kyiv"));
shouldBeFalse(all.includes("Europe/Kiev"));
shouldBeTrue(all.includes("America/Argentina/Buenos_Aires"));
shouldBeFalse(all.includes("America/Buenos_Aires"));

// Concern (1): Even though we now feed ICU the IANA primary ID rather than the
// CLDR canonical, ICU must still resolve correct offsets, DST transitions, and
// localized zone names. A legacy input and its primary must format identically.
function fmt(tz, instant) {
    const f = new Intl.DateTimeFormat("en", {
        timeZone: tz,
        timeZoneName: "long",
        year: "numeric", month: "2-digit", day: "2-digit",
        hour: "2-digit", minute: "2-digit", second: "2-digit",
        hour12: false,
    });
    return f.format(instant);
}
const summer = new Date("2024-06-15T12:00:00Z").getTime();
const winter = new Date("2024-01-15T12:00:00Z").getTime();
for (const t of [summer, winter]) {
    shouldBe(fmt("Asia/Calcutta", t),         fmt("Asia/Kolkata", t));
    shouldBe(fmt("America/Buenos_Aires", t),  fmt("America/Argentina/Buenos_Aires", t));
    shouldBe(fmt("Europe/Kiev", t),           fmt("Europe/Kyiv", t));
    shouldBe(fmt("Asia/Katmandu", t),         fmt("Asia/Kathmandu", t));
    shouldBe(fmt("US/Pacific", t),            fmt("America/Los_Angeles", t));
    shouldBe(fmt("GB", t),                    fmt("Europe/London", t));
    shouldBe(fmt("Brazil/East", t),           fmt("America/Sao_Paulo", t));
}

// Concern (2): Legacy non-primary IANA names must still be accepted as input.
const legacy = [
    "Asia/Calcutta", "America/Buenos_Aires", "Europe/Kiev", "Asia/Katmandu",
    "US/Pacific", "US/Eastern", "GB", "Brazil/East", "Canada/Eastern",
    "Australia/ACT", "UCT", "Universal", "Zulu", "GMT",
];
for (const tz of legacy) {
    // Should not throw on either Intl.DateTimeFormat or Date.prototype.toLocaleString.
    new Intl.DateTimeFormat("en", { timeZone: tz });
    new Date(0).toLocaleString("en", { timeZone: tz });
}

// Temporal.TimeZone uses the same hashmap-backed TimeZoneID lookup as Intl, so
// legacy aliases must also be accepted there. Unlike Intl.DateTimeFormat,
// Temporal.TimeZone.id surfaces the canonicalized primary identifier.
if (typeof Temporal !== "undefined") {
    const pairs = [
        ["Asia/Calcutta",          "Asia/Kolkata"],
        ["America/Buenos_Aires",   "America/Argentina/Buenos_Aires"],
        ["Europe/Kiev",            "Europe/Kyiv"],
        ["Asia/Katmandu",          "Asia/Kathmandu"],
        ["US/Pacific",             "America/Los_Angeles"],
        ["GB",                     "Europe/London"],
        ["Brazil/East",            "America/Sao_Paulo"],
        ["UCT",                    "UTC"],
        ["Etc/UTC",                "UTC"],
        ["Zulu",                   "UTC"],
    ];
    for (const [legacy, primary] of pairs) {
        shouldBe(new Temporal.TimeZone(legacy).id, primary);
        shouldBe(Temporal.TimeZone.from(legacy).id, primary);
    }
}
