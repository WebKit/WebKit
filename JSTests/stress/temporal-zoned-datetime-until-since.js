//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected "${expected}" but got "${actual}"`);
}
function shouldThrow(fn, errorType, msg) {
    let caught;
    try { fn(); } catch (e) { caught = e; }
    if (!caught) throw new Error(`${msg}: expected ${errorType.name}, no throw`);
    if (!(caught instanceof errorType))
        throw new Error(`${msg}: expected ${errorType.name} but got ${caught.constructor.name}`);
}

// ------------------------------------------------------------------
// (a) same-tz + time-largestUnit
// ------------------------------------------------------------------
{
    const a = Temporal.ZonedDateTime.from("2024-01-01T00:00:00-05:00[America/New_York]");
    const b = Temporal.ZonedDateTime.from("2024-01-01T03:15:30-05:00[America/New_York]");

    shouldBe(a.until(b).toString(), "PT3H15M30S", "(a) default (auto→hour) largestUnit");
    shouldBe(a.until(b, { largestUnit: "hour" }).toString(), "PT3H15M30S", "(a) largestUnit=hour");
    shouldBe(a.until(b, { largestUnit: "minute" }).toString(), "PT195M30S", "(a) largestUnit=minute");
    shouldBe(a.until(b, { largestUnit: "second" }).toString(), "PT11730S", "(a) largestUnit=second");

    // since = -until
    shouldBe(a.since(b).toString(), "-PT3H15M30S", "(a) since = negated until");
    shouldBe(b.since(a).toString(), "PT3H15M30S", "(a) since with reversed operands");
}

// ------------------------------------------------------------------
// (b) same-tz + date-largestUnit
// ------------------------------------------------------------------
{
    const a = Temporal.ZonedDateTime.from("2024-01-15T00:00:00-05:00[America/New_York]");
    const b = Temporal.ZonedDateTime.from("2024-04-20T12:30:00-04:00[America/New_York]");

    shouldBe(a.until(b, { largestUnit: "day" }).toString(), "P96DT12H30M", "(b) largestUnit=day");
    shouldBe(a.until(b, { largestUnit: "month" }).toString(), "P3M5DT12H30M", "(b) largestUnit=month");
    shouldBe(a.until(b, { largestUnit: "year" }).toString(), "P3M5DT12H30M", "(b) largestUnit=year (<1yr → month)");
    shouldBe(a.until(b, { largestUnit: "week" }).toString(), "P13W5DT12H30M", "(b) largestUnit=week");
}

// ------------------------------------------------------------------
// (c) cross-tz + time-largestUnit — spec allows this (Step 5 fast path)
// ------------------------------------------------------------------
{
    const nyc = Temporal.ZonedDateTime.from("2024-06-01T12:00:00-04:00[America/New_York]");
    const tky = Temporal.ZonedDateTime.from("2024-06-02T05:00:00+09:00[Asia/Tokyo]");
    // Same instant: NYC 12:00 EDT (UTC-4) = Tokyo 01:00 JST (UTC+9) next day.
    // 2024-06-02T05:00:00 JST = 2024-06-01T20:00 UTC = 2024-06-01T16:00 EDT → 4h after nyc.
    shouldBe(nyc.until(tky, { largestUnit: "hour" }).toString(), "PT4H", "(c) cross-tz hour diff");
    shouldBe(nyc.until(tky, { largestUnit: "minute" }).toString(), "PT240M", "(c) cross-tz minute diff");
    shouldBe(nyc.until(tky, { largestUnit: "second" }).toString(), "PT14400S", "(c) cross-tz second diff");
}

// ------------------------------------------------------------------
// (d) cross-tz + date-largestUnit — spec Step 7 RangeError
// ------------------------------------------------------------------
{
    const nyc = Temporal.ZonedDateTime.from("2024-06-01T00:00:00-04:00[America/New_York]");
    const tky = Temporal.ZonedDateTime.from("2024-07-01T00:00:00+09:00[Asia/Tokyo]");

    shouldThrow(() => nyc.until(tky, { largestUnit: "day" }), RangeError,   "(d) day across tz");
    shouldThrow(() => nyc.until(tky, { largestUnit: "week" }), RangeError,  "(d) week across tz");
    shouldThrow(() => nyc.until(tky, { largestUnit: "month" }), RangeError, "(d) month across tz");
    shouldThrow(() => nyc.until(tky, { largestUnit: "year" }), RangeError,  "(d) year across tz");

    shouldThrow(() => nyc.since(tky, { largestUnit: "day" }),   RangeError, "(d) since day across tz");
    shouldThrow(() => nyc.since(tky, { largestUnit: "month" }), RangeError, "(d) since month across tz");
}

// ------------------------------------------------------------------
// (e) epochNs-equal — zero duration (all 10 fields zero)
// ------------------------------------------------------------------
{
    const a = Temporal.ZonedDateTime.from("2024-06-01T12:00:00-04:00[America/New_York]");
    // Same instant, expressed in a different zone.
    const b = Temporal.ZonedDateTime.from("2024-06-02T01:00:00+09:00[Asia/Tokyo]");
    // Verify same instant
    if (a.epochNanoseconds !== b.epochNanoseconds)
        throw new Error("(e) precondition: a and b must have equal epochNanoseconds");

    // For time-largestUnit: same-instant across zones → zero (fast-path spec Step 5).
    for (const largestUnit of ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]) {
        shouldBe(a.until(b, { largestUnit }).toString(), "PT0S", `(e) equal-epoch time-largestUnit ${largestUnit}`);
        shouldBe(a.since(b, { largestUnit }).toString(), "PT0S", `(e) equal-epoch time-largestUnit ${largestUnit} since`);
    }

    // For date-largestUnit + same-tz + same epoch: also zero.
    const c = Temporal.ZonedDateTime.from("2024-06-01T12:00:00-04:00[America/New_York]");
    shouldBe(a.until(c, { largestUnit: "day" }).toString(),   "PT0S", "(e) same-tz day");
    shouldBe(a.until(c, { largestUnit: "month" }).toString(), "PT0S", "(e) same-tz month");
    shouldBe(a.until(c, { largestUnit: "year" }).toString(),  "PT0S", "(e) same-tz year");
    shouldBe(a.since(c, { largestUnit: "year" }).toString(),  "PT0S", "(e) same-tz year since");

    // Additional: verify Duration has ALL 10 fields = 0 (spec's Step 8 CreateTemporalDuration(0,0,0,0,0,0,0,0,0,0)).
    const zero = a.until(c, { largestUnit: "year" });
    for (const f of ["years", "months", "weeks", "days", "hours", "minutes", "seconds",
                     "milliseconds", "microseconds", "nanoseconds"]) {
        if (zero[f] !== 0)
            throw new Error(`(e) zero-duration field ${f} = ${zero[f]}, expected 0`);
    }
}

// ------------------------------------------------------------------
// Sanity: calendar mismatch throws (spec Step 2)
// ------------------------------------------------------------------
{
    const iso = Temporal.ZonedDateTime.from("2024-06-01T00:00:00+00:00[UTC]");
    const heb = Temporal.ZonedDateTime.from("2024-06-01T00:00:00+00:00[UTC][u-ca=hebrew]");
    shouldThrow(() => iso.until(heb),  RangeError, "Step 2: different calendars → RangeError");
    shouldThrow(() => heb.since(iso), RangeError, "Step 2: different calendars (since) → RangeError");
}

print("PASS");
