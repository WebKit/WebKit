//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected "${expected}" but got "${actual}"`);
}
function shouldThrow(fn, errorType, msg) {
    let caught;
    try { fn(); } catch (e) { caught = e; }
    if (!caught) throw new Error(`${msg}: expected ${errorType.name}`);
    if (!(caught instanceof errorType)) throw new Error(`${msg}: expected ${errorType.name} but got ${caught.constructor.name}`);
}

// Base instant used throughout: 2024-06-15 13:24:36.123_456_789 (UTC-only ISO calendar).
const base = new Temporal.PlainDateTime(2024, 6, 15, 13, 24, 36, 123, 456, 789);

// --- Default mode is halfExpand. Validate every smallestUnit against the spec semantics. ---
shouldBe(base.round("day").toString(), "2024-06-16T00:00:00", "round day default");
shouldBe(base.round("hour").toString(), "2024-06-15T13:00:00", "round hour default");
shouldBe(base.round("minute").toString(), "2024-06-15T13:25:00", "round minute default");
shouldBe(base.round("second").toString(), "2024-06-15T13:24:36", "round second default");
shouldBe(base.round("millisecond").toString(), "2024-06-15T13:24:36.123", "round ms default (.456789 < .5)");
shouldBe(base.round("microsecond").toString(), "2024-06-15T13:24:36.123457", "round us default (.789 > .5)");
shouldBe(base.round("nanosecond").toString(), "2024-06-15T13:24:36.123456789", "round ns identity");

// --- All rounding modes against the same input for one unit (millisecond).
//     Input fractional remainder below the ms boundary is 0.456789 ms, which is < 0.5,
//     so every half-* mode rounds toward the floor (.123); only ceil/expand bump to .124. ---
const ms = (mode) => base.round({ smallestUnit: "millisecond", roundingMode: mode }).toString();
shouldBe(ms("halfExpand"), "2024-06-15T13:24:36.123", "ms halfExpand");
shouldBe(ms("ceil"), "2024-06-15T13:24:36.124", "ms ceil");
shouldBe(ms("floor"), "2024-06-15T13:24:36.123", "ms floor");
shouldBe(ms("trunc"), "2024-06-15T13:24:36.123", "ms trunc");
shouldBe(ms("halfCeil"), "2024-06-15T13:24:36.123", "ms halfCeil");
shouldBe(ms("halfFloor"), "2024-06-15T13:24:36.123", "ms halfFloor");
shouldBe(ms("halfTrunc"), "2024-06-15T13:24:36.123", "ms halfTrunc");
shouldBe(ms("halfEven"), "2024-06-15T13:24:36.123", "ms halfEven");
shouldBe(ms("expand"), "2024-06-15T13:24:36.124", "ms expand");

// --- Rounding increments > 1 (must divide the maximum). ---
shouldBe(base.round({ smallestUnit: "minute", roundingIncrement: 30 }).toString(),
    "2024-06-15T13:30:00", "minute increment 30");
shouldBe(base.round({ smallestUnit: "second", roundingIncrement: 15 }).toString(),
    "2024-06-15T13:24:30", "second increment 15 (36 < 37.5)");
shouldBe(base.round({ smallestUnit: "hour", roundingIncrement: 6 }).toString(),
    "2024-06-15T12:00:00", "hour increment 6 (13 < 15)");

// --- Boundary: rounding up across midnight bumps the date (day rollover). ---
const lateNight = new Temporal.PlainDateTime(2024, 6, 15, 23, 59, 59, 999, 999, 999);
shouldBe(lateNight.round("minute").toString(), "2024-06-16T00:00:00",
    "halfExpand minute crosses midnight → next day");
shouldBe(lateNight.round({ smallestUnit: "second", roundingMode: "ceil" }).toString(),
    "2024-06-16T00:00:00", "ceil second crosses month boundary");

// --- Day-rollover crossing month/year boundary. ---
const lastDayOfYear = new Temporal.PlainDateTime(2024, 12, 31, 23, 59, 59, 999, 999, 999);
shouldBe(lastDayOfYear.round("hour").toString(), "2025-01-01T00:00:00",
    "year-end rollover via hour rounding");

// --- Calendar preservation across round. ---
const heb = new Temporal.PlainDateTime(2024, 1, 1, 13, 30, 0, 0, 0, 0, "hebrew");
const roundedHeb = heb.round("hour");
shouldBe(roundedHeb.calendarId, "hebrew", "round preserves hebrew calendar id");
shouldBe(roundedHeb.toString(), "2024-01-01T14:00:00[u-ca=hebrew]", "round preserves hebrew toString");

// --- String shorthand path: `pdt.round("hour")` ≡ `pdt.round({ smallestUnit: "hour" })`. ---
shouldBe(base.round("hour").toString(), base.round({ smallestUnit: "hour" }).toString(), "string vs obj — hour");
shouldBe(base.round("nanosecond").toString(), base.round({ smallestUnit: "nanosecond" }).toString(), "string vs obj — nanosecond");

// --- Error paths. ---
shouldThrow(() => base.round(), TypeError, "no arg → TypeError (Step 3)");
shouldThrow(() => base.round(undefined), TypeError, "explicit undefined → TypeError");
shouldThrow(() => base.round({}), RangeError, "missing smallestUnit (Required) → RangeError");
shouldThrow(() => base.round({ smallestUnit: "year" }), RangeError, "year disallowed → RangeError");
shouldThrow(() => base.round({ smallestUnit: "month" }), RangeError, "month disallowed → RangeError");
shouldThrow(() => base.round({ smallestUnit: "week" }), RangeError, "week disallowed → RangeError");
shouldThrow(() => base.round({ smallestUnit: "junk" }), RangeError, "unknown unit → RangeError");
shouldThrow(() => base.round("year"), RangeError, "string year disallowed");
shouldThrow(() => base.round("junk"), RangeError, "string junk disallowed");
// Day is inclusive-bounded at 1; 2 is rejected.
shouldThrow(() => base.round({ smallestUnit: "day", roundingIncrement: 2 }), RangeError, "day increment > 1 → RangeError");
// Minute max is 60 exclusive; an indivisor like 7 fails.
shouldThrow(() => base.round({ smallestUnit: "minute", roundingIncrement: 7 }), RangeError, "minute increment 7 (non-divisor of 60) → RangeError");
