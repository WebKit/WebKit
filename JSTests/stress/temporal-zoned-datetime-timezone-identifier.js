//@ requireOptions("--useTemporal=1")

// Coverage for alias-preserving [[TimeZone]] identifiers in Temporal.ZonedDateTime
// and the spec-aligned TimeZoneEquals algorithm
// (https://tc39.es/proposal-canonical-tz/#sec-temporal-timezoneequals).

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
}

function shouldBeTrue(v, msg) { if (v !== true) throw new Error(`${msg}: expected true`); }
function shouldBeFalse(v, msg) { if (v !== false) throw new Error(`${msg}: expected false`); }

// Bracket annotation preserves "+00:00" as distinct from "UTC". The offset designator
// (Z or +HH:MM) only contributes to the instant; only the bracket sets [[TimeZone]].
{
    const cases = [
        ["1970-01-01T00:00[UTC]", "UTC"],
        ["1970-01-01T00:00[!UTC]", "UTC"],
        ["1970-01-01T00:00[+00:00]", "+00:00"],
        ["1970-01-01T00:00[!+00:00]", "+00:00"],
        ["1970-01-01T00:00Z[UTC]", "UTC"],
        ["1970-01-01T00:00Z[+00:00]", "+00:00"],
        ["1970-01-01T00:00+00:00[UTC]", "UTC"],
        ["1970-01-01T00:00+00:00[+00:00]", "+00:00"],
    ];
    for (const [arg, expected] of cases)
        shouldBe(Temporal.ZonedDateTime.from(arg).timeZoneId, expected, `from(${arg}).timeZoneId`);
}

// Backward-link aliases survive verbatim: timeZoneId returns the alias text, not
// the canonical IANA primary.
{
    const aliases = [
        "Asia/Calcutta",
        "Asia/Kolkata",
        "America/Buenos_Aires",
        "Europe/Kiev",
        "Etc/UTC",
        "Etc/GMT",
        "GMT",
        "Greenwich",
        "Universal",
        "Zulu",
    ];
    for (const id of aliases)
        shouldBe(new Temporal.ZonedDateTime(0n, id).timeZoneId, id, `${id} preserved`);
}

// Case normalization: input case folded to ICU's canonical case for the identifier;
// supports both primaries (UTC) and aliases (asia/calcutta).
shouldBe(new Temporal.ZonedDateTime(0n, "utc").timeZoneId, "UTC", "lowercase utc → UTC");
shouldBe(new Temporal.ZonedDateTime(0n, "asia/calcutta").timeZoneId, "Asia/Calcutta", "lowercase alias case-normalized");
shouldBe(new Temporal.ZonedDateTime(0n, "ASIA/KOLKATA").timeZoneId, "Asia/Kolkata", "uppercase primary case-normalized");

// Offset canonicalization: any input form folds to +HH:MM[:SS[.fff]] per
// FormatOffsetTimeZoneIdentifier. -00:00 collapses to +00:00 (offsetMinutes = 0).
shouldBe(new Temporal.ZonedDateTime(0n, "+0530").timeZoneId, "+05:30", "+0530 → +05:30");
shouldBe(new Temporal.ZonedDateTime(0n, "+05").timeZoneId, "+05:00", "+05 → +05:00");
shouldBe(new Temporal.ZonedDateTime(0n, "-00:00").timeZoneId, "+00:00", "-00:00 → +00:00 (negative zero)");
shouldBe(new Temporal.ZonedDateTime(0n, "+00:00").timeZoneId, "+00:00", "+00:00 stays +00:00");

// Z designator (no bracket) parsed as full datetime → [[TimeZone]] = "UTC" (named),
// per ParseTemporalTimeZoneString step 6: "If timeZoneResult.[[Z]] is true, return
// ! ParseTimeZoneIdentifier('UTC')."
{
    const z = new Temporal.ZonedDateTime(0n, "UTC").withTimeZone("1994-11-05T13:15:30Z");
    shouldBe(z.timeZoneId, "UTC", "Z designator yields named UTC, not +00:00");
}

// equals(): both named → primary comparison. Aliases of the same primary are equal;
// aliases of different primaries (or named vs offset) are not.
{
    const calcutta = new Temporal.ZonedDateTime(0n, "Asia/Calcutta");
    const kolkata = new Temporal.ZonedDateTime(0n, "Asia/Kolkata");
    shouldBeTrue(calcutta.equals(kolkata), "Asia/Calcutta equals Asia/Kolkata (same primary)");
    shouldBeTrue(kolkata.equals(calcutta), "Asia/Kolkata equals Asia/Calcutta (commutative)");

    const utc = new Temporal.ZonedDateTime(0n, "UTC");
    const etcUtc = new Temporal.ZonedDateTime(0n, "Etc/UTC");
    const universal = new Temporal.ZonedDateTime(0n, "Universal");
    shouldBeTrue(utc.equals(etcUtc), "UTC equals Etc/UTC");
    shouldBeTrue(utc.equals(universal), "UTC equals Universal");

    const ny = new Temporal.ZonedDateTime(0n, "America/New_York");
    shouldBeFalse(utc.equals(ny), "UTC not equals America/New_York");
}

// equals(): both offset → numeric comparison.
{
    const a = new Temporal.ZonedDateTime(0n, "+05:30");
    const b = new Temporal.ZonedDateTime(0n, "+0530");
    const c = new Temporal.ZonedDateTime(0n, "+05:00");
    shouldBeTrue(a.equals(b), "+05:30 equals +0530 (same canonical offset)");
    shouldBeFalse(a.equals(c), "+05:30 not equals +05:00");
}

// equals(): named vs offset → false even when offsets coincide. "UTC" is a named TZ
// that happens to be at offset 0; "+00:00" is an offset TZ. Per spec they are distinct.
{
    const named = new Temporal.ZonedDateTime(0n, "UTC");
    const offset = new Temporal.ZonedDateTime(0n, "+00:00");
    shouldBeFalse(named.equals(offset), "UTC not equals +00:00 (named vs offset)");
    shouldBeFalse(offset.equals(named), "+00:00 not equals UTC (commutative)");
}

// withCalendar preserves the original alias identifier (no canonicalization).
{
    const z = new Temporal.ZonedDateTime(0n, "Asia/Calcutta").withCalendar("iso8601");
    shouldBe(z.timeZoneId, "Asia/Calcutta", "withCalendar preserves alias");
}

// withTimeZone with an offset-string argument produces an offset TZ.
{
    const z = new Temporal.ZonedDateTime(0n, "UTC").withTimeZone("+05:30");
    shouldBe(z.timeZoneId, "+05:30", "withTimeZone(offset)");
}

// withTimeZone with a ZonedDateTime argument adopts its [[TimeZone]] verbatim.
{
    const src = new Temporal.ZonedDateTime(0n, "Asia/Calcutta");
    const dst = new Temporal.ZonedDateTime(0n, "UTC").withTimeZone(src);
    shouldBe(dst.timeZoneId, "Asia/Calcutta", "withTimeZone(ZDT) adopts alias");
}

// toLocaleString GMT fixup applies only to +00:00 (offset 0) — not to named UTC and
// not to non-zero offsets (ICU formats those as GMT±HH:MM directly).
{
    const offsetUtc = new Temporal.ZonedDateTime(0n, "+00:00").toLocaleString("en-US", { timeZoneName: "short" });
    if (!offsetUtc.includes("GMT") || offsetUtc.includes("UTC"))
        throw new Error(`+00:00 toLocaleString should contain "GMT" not "UTC", got: ${offsetUtc}`);

    const namedUtc = new Temporal.ZonedDateTime(0n, "UTC").toLocaleString("en-US", { timeZoneName: "short" });
    if (!namedUtc.includes("UTC"))
        throw new Error(`UTC toLocaleString should contain "UTC", got: ${namedUtc}`);

    const offset530 = new Temporal.ZonedDateTime(0n, "+05:30").toLocaleString("en-US", { timeZoneName: "short" });
    if (!offset530.includes("GMT+5:30") && !offset530.includes("GMT+05:30"))
        throw new Error(`+05:30 toLocaleString should contain "GMT+5:30" or "GMT+05:30", got: ${offset530}`);
}

// since/until with day-or-larger units rejects ZDTs whose time zones differ; aliases
// of the same primary are accepted (same-primary check goes through TimeZoneEquals).
{
    const a = new Temporal.ZonedDateTime(0n, "Asia/Calcutta");
    const b = new Temporal.ZonedDateTime(86400000000000n, "Asia/Kolkata");
    // Should not throw — same primary.
    a.since(b, { largestUnit: "day" });
    a.until(b, { largestUnit: "day" });

    const c = new Temporal.ZonedDateTime(86400000000000n, "America/New_York");
    let threw = false;
    try { a.since(c, { largestUnit: "day" }); } catch (e) { threw = true; }
    if (!threw)
        throw new Error("since() across different primaries should throw with day-largest unit");
}

// Intl.supportedValuesOf("timeZone") returns primaries only — aliases excluded.
{
    const tzs = Intl.supportedValuesOf("timeZone");
    if (!tzs.includes("UTC"))
        throw new Error(`supportedValuesOf("timeZone") should include "UTC"`);
    if (!tzs.includes("Asia/Kolkata"))
        throw new Error(`supportedValuesOf("timeZone") should include "Asia/Kolkata"`);
    if (tzs.includes("Asia/Calcutta"))
        throw new Error(`supportedValuesOf("timeZone") must exclude alias "Asia/Calcutta"`);
    if (tzs.includes("Etc/UTC"))
        throw new Error(`supportedValuesOf("timeZone") must exclude alias "Etc/UTC"`);
}
