//@ requireOptions("--useTemporal=1")

// Stress test for ParseISODateTime — https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime
// Each section covers one spec step / sub-step. Test inputs are routed through the consumer
// entry points that invoke parseISODateTime with the corresponding production mask:
//   Temporal.Instant.from           → mask = { Instant }
//   Temporal.PlainDate.from         → mask = { DateTimeUnzoned }
//   Temporal.PlainDateTime.from     → mask = { DateTimeUnzoned }
//   Temporal.PlainYearMonth.from    → mask = { YearMonth }
//   Temporal.PlainMonthDay.from     → mask = { MonthDay }
//   Temporal.PlainTime.from         → mask = { Time }
//   Temporal.ZonedDateTime.from     → mask = { DateTimeZoned }
//   property bag .calendar = string → full 6-production union (TemporalObject calendar canon).

function shouldThrow(fn, expectedType, label) {
    let error;
    try { fn(); } catch (e) { error = e; }
    if (!(error instanceof expectedType))
        throw new Error(`${label}: expected ${expectedType.name} but got ${error?.constructor.name ?? 'no error'}: ${error?.message ?? ''}`);
}
function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${String(expected)} but got ${String(actual)}`);
}
function shouldNotThrow(fn, label) {
    try { fn(); } catch (e) {
        throw new Error(`${label}: unexpected ${e.constructor.name}: ${e.message}`);
    }
}

// Helper: route a string through calendar canonicalization (full-union mask).
function calendarOf(s) {
    return Temporal.PlainMonthDay.from({ monthCode: "M11", day: 18, calendar: s }).calendarId;
}

// =============================================================================
// Step 4 — goal dispatch: each TemporalXxxString matches its inputs.
// =============================================================================

// TemporalInstantString grammar: Date DateTimeSep Time DateTimeUTCOffset[+Z] TZAnno? Annotations?
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z").epochNanoseconds, 1705320000000000000n, "Instant: Z form");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:00").epochNanoseconds, 1705302000000000000n, "Instant: numeric offset");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z[UTC]").epochNanoseconds, 1705320000000000000n, "Instant: Z + bracket");
shouldThrow(() => Temporal.Instant.from("2024-01-15"), RangeError, "Instant: bare Date rejected");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00"), RangeError, "Instant: missing Z/offset");
shouldThrow(() => Temporal.Instant.from("12:00:00Z"), RangeError, "Instant: missing Date");

// TemporalDateTimeString[~Zoned] — used by PlainDate and PlainDateTime; Z forbidden.
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15"), "PlainDate: bare Date OK");
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15T12:00:00"), "PlainDate: Date+Time OK");
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15T12:00:00+05:00"), "PlainDate: Date+Time+offset OK");
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15[America/New_York]"), "PlainDate: bare Date+TZ OK");
shouldThrow(() => Temporal.PlainDate.from("2024-01-15T12:00:00Z"), RangeError, "PlainDate: Z rejected");
shouldThrow(() => Temporal.PlainDate.from("2024-01-15T12:00:00Z[UTC]"), RangeError, "PlainDate: Z+bracket rejected");

// TemporalDateTimeString[+Zoned] — used by ZonedDateTime; bracket TZ required.
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00[America/New_York]"), "ZDT: Date+Time+TZ OK");
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00Z[UTC]"), "ZDT: Z+TZ OK");
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00-05:00[America/New_York]"), "ZDT: offset+TZ OK");
shouldThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00Z"), RangeError, "ZDT: bare Z (no bracket)");
shouldThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00"), RangeError, "ZDT: no offset, no bracket");
shouldThrow(() => Temporal.ZonedDateTime.from("2024-01-15"), RangeError, "ZDT: bare Date (no time, no bracket)");

// TemporalYearMonthString — short form OR full datetime.
shouldNotThrow(() => Temporal.PlainYearMonth.from("2024-01"), "YearMonth: short hyphenated");
shouldNotThrow(() => Temporal.PlainYearMonth.from("202401"), "YearMonth: short compact");
shouldNotThrow(() => Temporal.PlainYearMonth.from("2024-01-15"), "YearMonth: full date fallback");
shouldNotThrow(() => Temporal.PlainYearMonth.from("2024-01-15T12:00:00"), "YearMonth: full datetime fallback");

// TemporalMonthDayString — short form OR full datetime.
shouldNotThrow(() => Temporal.PlainMonthDay.from("01-15"), "MonthDay: short hyphenated");
shouldNotThrow(() => Temporal.PlainMonthDay.from("0115"), "MonthDay: short compact");
shouldNotThrow(() => Temporal.PlainMonthDay.from("--01-15"), "MonthDay: short with -- prefix");
shouldNotThrow(() => Temporal.PlainMonthDay.from("--0115"), "MonthDay: short compact with -- prefix");
shouldNotThrow(() => Temporal.PlainMonthDay.from("2024-01-15"), "MonthDay: full date fallback");
shouldNotThrow(() => Temporal.PlainMonthDay.from("2024-01-15T12:00:00"), "MonthDay: full datetime fallback");

// TemporalTimeString — AnnotatedTime OR datetime with time.
shouldNotThrow(() => Temporal.PlainTime.from("12:00"), "Time: HH:MM");
shouldNotThrow(() => Temporal.PlainTime.from("12:00:00"), "Time: HH:MM:SS");
shouldNotThrow(() => Temporal.PlainTime.from("120000"), "Time: HHMMSS compact");
shouldNotThrow(() => Temporal.PlainTime.from("T12:00"), "Time: with TimeDesignator");
shouldNotThrow(() => Temporal.PlainTime.from("2024-01-15T12:00:00"), "Time: datetime fallback");
shouldThrow(() => Temporal.PlainTime.from("12:00:00Z"), RangeError, "Time: Z forbidden");
shouldThrow(() => Temporal.PlainTime.from("2024-01-15"), RangeError, "Time: bare Date rejected");

// =============================================================================
// Step 4.a.ii.(1)–(2) — annotation processing & critical-flag rules.
// =============================================================================

// "u-ca" annotation extracted as calendar.
shouldBe(calendarOf("2020-01-15[u-ca=hebrew]"), "hebrew", "annotation: u-ca extracted");
shouldBe(calendarOf("2020-01-15"), "iso8601", "annotation: missing → iso8601");

// Critical flag on u-ca: allowed (single).
shouldBe(calendarOf("2020-01-15[!u-ca=hebrew]"), "hebrew", "annotation: critical u-ca single");

// Critical flag on unknown key: RangeError.
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[!foo=bar]"), RangeError,
    "annotation: critical unknown key");
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[u-ca=iso8601][!foo=bar]"), RangeError,
    "annotation: critical unknown key after calendar");

// Multiple calendar annotations: only one calendar kept; if EITHER is critical AND not the only one, RangeError.
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[!u-ca=hebrew][u-ca=iso8601]"), RangeError,
    "annotation: critical first calendar + duplicate");
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[u-ca=hebrew][!u-ca=iso8601]"), RangeError,
    "annotation: critical second calendar");

// Non-critical unknown annotation: silently ignored.
shouldNotThrow(() => Temporal.PlainDate.from("2020-01-15[foo=bar]"),
    "annotation: non-critical unknown key OK");

// =============================================================================
// Step 4.a.ii.(3) — TemporalYearMonthString + no DateDay → calendar must be iso8601.
// =============================================================================

shouldNotThrow(() => Temporal.PlainYearMonth.from("2020-01"),
    "YM short: no calendar OK");
shouldNotThrow(() => Temporal.PlainYearMonth.from("2020-01[u-ca=iso8601]"),
    "YM short: iso8601 calendar OK");
shouldThrow(() => Temporal.PlainYearMonth.from("2020-01[u-ca=hebrew]"), RangeError,
    "YM short: non-iso calendar rejected");
// Long form bypasses the early-error: full date allows non-iso calendar.
shouldNotThrow(() => Temporal.PlainYearMonth.from("2020-01-15[u-ca=hebrew]"),
    "YM long form: non-iso calendar OK");

// =============================================================================
// Step 4.a.ii.(4) — TemporalMonthDayString + no DateYear → calendar must be iso8601; yearAbsent.
// =============================================================================

shouldNotThrow(() => Temporal.PlainMonthDay.from("01-15"),
    "MD short: no calendar OK");
shouldNotThrow(() => Temporal.PlainMonthDay.from("01-15[u-ca=iso8601]"),
    "MD short: iso8601 calendar OK");
shouldThrow(() => Temporal.PlainMonthDay.from("01-15[u-ca=hebrew]"), RangeError,
    "MD short: non-iso calendar rejected");
shouldNotThrow(() => Temporal.PlainMonthDay.from("2020-01-15[u-ca=hebrew]"),
    "MD long form: non-iso calendar OK");

// Property-bag .calendar route: short-form non-iso calendar string also rejected
// (routes through ToTemporalCalendarIdentifier → ParseTemporalCalendarString).
shouldThrow(() => Temporal.PlainDate.from({ year: 2020, month: 1, day: 15, calendar: "2020-01[u-ca=hebrew]" }),
    RangeError, "calendar property: short-YM non-iso rejected");
shouldThrow(() => Temporal.PlainDate.from({ year: 2020, month: 1, day: 15, calendar: "01-15[u-ca=hebrew]" }),
    RangeError, "calendar property: short-MD non-iso rejected");
shouldThrow(() => Temporal.PlainDate.from({ year: 2020, month: 1, day: 15, calendar: "--01-15[u-ca=hebrew]" }),
    RangeError, "calendar property: short-MD basic non-iso rejected");
// Full-form date with non-iso calendar in property bag is fine (no Step 4.a.ii.(3)/(4) violation).
shouldNotThrow(() => Temporal.PlainDate.from({ year: 2020, month: 1, day: 15, calendar: "2020-01-15[u-ca=hebrew]" }),
    "calendar property: full-form non-iso OK");

// =============================================================================
// Step 5 — no goal matched → RangeError.
// =============================================================================

shouldThrow(() => Temporal.PlainDate.from(""), RangeError, "Step 5: empty string");
shouldThrow(() => Temporal.PlainDate.from("garbage"), RangeError, "Step 5: garbage");
shouldThrow(() => Temporal.PlainDate.from("2020-13-01"), RangeError, "Step 5: month=13");
shouldThrow(() => Temporal.PlainDate.from("2020-00-01"), RangeError, "Step 5: month=0");
shouldThrow(() => Temporal.PlainDate.from("2020-01-32"), RangeError, "Step 5: day=32");
shouldThrow(() => Temporal.PlainDate.from("2024-02-30"), RangeError, "Step 5: Feb 30");
shouldThrow(() => Temporal.PlainDate.from("2020-01-15extra"), RangeError, "Step 5: trailing junk");

// =============================================================================
// Step 8 — yearMV (DateYear: 4-digit OR ±6-digit extended).
// =============================================================================

shouldNotThrow(() => Temporal.PlainDate.from("0000-01-01"), "year: 0000");
shouldNotThrow(() => Temporal.PlainDate.from("9999-12-31"), "year: 9999");
shouldNotThrow(() => Temporal.PlainDate.from("+275760-09-13"), "year: max valid extended");
shouldNotThrow(() => Temporal.PlainDate.from("-271821-04-20"), "year: min valid extended");
shouldThrow(() => Temporal.PlainDate.from("-000000-01-01"), RangeError, "year: -000000 forbidden");
shouldNotThrow(() => Temporal.PlainDate.from("+000000-01-01"), "year: +000000 = 0");
shouldThrow(() => Temporal.PlainDate.from("123-01-01"), RangeError, "year: 3-digit not allowed");

// =============================================================================
// Steps 9–10 — monthMV (DateMonth: 01-12).
// =============================================================================

for (let m = 1; m <= 12; ++m) {
    const mm = String(m).padStart(2, "0");
    shouldNotThrow(() => Temporal.PlainDate.from(`2024-${mm}-01`), `month=${mm}`);
}
shouldThrow(() => Temporal.PlainDate.from("2024-00-01"), RangeError, "month=00");
shouldThrow(() => Temporal.PlainDate.from("2024-13-01"), RangeError, "month=13");

// =============================================================================
// Steps 11–12 — dayMV (DateDay: 01..daysInMonth).
// =============================================================================

shouldNotThrow(() => Temporal.PlainDate.from("2024-02-29"), "Feb 29 in leap year");
shouldThrow(() => Temporal.PlainDate.from("2023-02-29"), RangeError, "Feb 29 in non-leap year");
shouldNotThrow(() => Temporal.PlainDate.from("2024-04-30"), "Apr 30");
shouldThrow(() => Temporal.PlainDate.from("2024-04-31"), RangeError, "Apr 31");
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-31"), "Jan 31");
shouldThrow(() => Temporal.PlainDate.from("2024-01-32"), RangeError, "Jan 32");
shouldThrow(() => Temporal.PlainDate.from("2024-01-00"), RangeError, "Jan 00");

// =============================================================================
// Steps 13–18 — hour/minute/second + leap-second clamp.
// =============================================================================

for (let h = 0; h <= 23; ++h)
    shouldNotThrow(() => Temporal.PlainTime.from(`${String(h).padStart(2, "0")}:00`), `hour=${h}`);
shouldThrow(() => Temporal.PlainTime.from("24:00"), RangeError, "hour=24");
shouldThrow(() => Temporal.PlainTime.from("25:00"), RangeError, "hour=25");

shouldNotThrow(() => Temporal.PlainTime.from("12:00"), "minute=00");
shouldNotThrow(() => Temporal.PlainTime.from("12:59"), "minute=59");
shouldThrow(() => Temporal.PlainTime.from("12:60"), RangeError, "minute=60");

shouldNotThrow(() => Temporal.PlainTime.from("12:00:00"), "second=00");
shouldNotThrow(() => Temporal.PlainTime.from("12:00:59"), "second=59");
// Leap-second: :60 is grammar-valid and clamped to :59 (Step 18.b).
shouldBe(Temporal.PlainTime.from("12:00:60").second, 59, "second=60 clamped to 59");
shouldThrow(() => Temporal.PlainTime.from("12:00:61"), RangeError, "second=61");

// =============================================================================
// Steps 19–20 — fractional seconds (1–9 digits, '.' or ',').
// =============================================================================

for (let d = 1; d <= 9; ++d) {
    const frac = "1".padEnd(d, "0");
    shouldNotThrow(() => Temporal.PlainTime.from(`12:00:00.${frac}`), `${d} fractional digits`);
}
shouldThrow(() => Temporal.PlainTime.from("12:00:00.1234567890"), RangeError, "10 fractional digits");
shouldThrow(() => Temporal.PlainTime.from("12:00:00."), RangeError, "trailing dot, no digits");

// Both '.' and ',' are valid separators.
shouldBe(Temporal.PlainTime.from("12:00:00,5").millisecond, 500, "comma fractional separator");

// Padding to 9 digits (ms / us / ns split).
const t = Temporal.PlainTime.from("12:00:00.123456789");
shouldBe(t.millisecond, 123, "fractional → ms");
shouldBe(t.microsecond, 456, "fractional → us");
shouldBe(t.nanosecond, 789, "fractional → ns");
const t2 = Temporal.PlainTime.from("12:00:00.1");
shouldBe(t2.millisecond, 100, "0.1 → 100 ms");
shouldBe(t2.microsecond, 0, "0.1 → 0 us");
shouldBe(t2.nanosecond, 0, "0.1 → 0 ns");

// =============================================================================
// Step 21 — IsValidISODate assert (covered by parseDate's bounds check above).
// =============================================================================

// Already exercised by Steps 9-12 tests. Add boundary year cases:
shouldNotThrow(() => Temporal.PlainDate.from("+275760-09-13"), "max ISO date");
// One-day-past-max: year extension still parses but downstream validation rejects.
shouldThrow(() => Temporal.PlainDate.from("+275760-09-14"), RangeError, "+1 past max");

// =============================================================================
// Steps 22–23 — time = start-of-day vs CreateTimeRecord.
// =============================================================================

// PlainDate accepts a bare Date (time absent → start-of-day in spec terms).
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15"), "PlainDate: bare Date");
// PlainDateTime with bare date → time defaults to 00:00:00.
const pdt = Temporal.PlainDateTime.from("2024-01-15");
shouldBe(pdt.hour, 0, "PlainDateTime bare date → hour=0");
shouldBe(pdt.minute, 0, "PlainDateTime bare date → minute=0");
shouldBe(pdt.second, 0, "PlainDateTime bare date → second=0");
// PlainDateTime with full datetime → time populated.
const pdt2 = Temporal.PlainDateTime.from("2024-01-15T17:30:45.123");
shouldBe(pdt2.hour, 17, "PlainDateTime full → hour=17");
shouldBe(pdt2.minute, 30, "PlainDateTime full → minute=30");
shouldBe(pdt2.second, 45, "PlainDateTime full → second=45");
shouldBe(pdt2.millisecond, 123, "PlainDateTime full → ms=123");

// =============================================================================
// Steps 24–27 — timeZoneResult ([[Z]], [[OffsetString]], [[TimeZoneAnnotation]]).
// =============================================================================

// Step 26: [[Z]] = true.
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z").epochNanoseconds, 1705320000000000000n,
    "Step 26: Z designator");

// Step 27: [[OffsetString]] from numeric offset.
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+00:00").epochNanoseconds, 1705320000000000000n,
    "Step 27: +00:00 == Z");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00-00:00").epochNanoseconds, 1705320000000000000n,
    "Step 27: -00:00 == Z");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+14:00").epochNanoseconds, 1705269600000000000n,
    "Step 27: max +14:00");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00-14:00").epochNanoseconds, 1705370400000000000n,
    "Step 27: min -14:00");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:30:15").epochNanoseconds, 1705300185000000000n,
    "Step 27: sub-minute precision (3-component)");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:30:15.123456789").epochNanoseconds, 1705300184876543211n,
    "Step 27: fractional offset");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05").epochNanoseconds, 1705302000000000000n,
    "Step 27: bare ±HH");

// Step 25: [[TimeZoneAnnotation]] from bracket.
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00[America/New_York]"),
    "Step 25: named bracket");
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00[+05:00]"),
    "Step 25: numeric bracket");
shouldNotThrow(() => Temporal.ZonedDateTime.from("2024-01-15T12:00:00[!America/New_York]"),
    "Step 25: critical named bracket");

// Steps 24-27 all empty (PlainDate without time/zone).
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15"), "Step 24: no TZ info");

// =============================================================================
// Step 28 — yearAbsent for MonthDay short form.
// =============================================================================

const md1 = Temporal.PlainMonthDay.from("--12-25");
shouldBe(md1.toString(), "12-25", "Step 28: --MM-DD short form");
const md2 = Temporal.PlainMonthDay.from("12-25");
shouldBe(md2.toString(), "12-25", "Step 28: MM-DD short form");
// Long form preserves date but PlainMonthDay still strips year.
const md3 = Temporal.PlainMonthDay.from("2024-12-25");
shouldBe(md3.toString(), "12-25", "Step 28: long form, year stripped");

// =============================================================================
// Step 29 — full record return (covered implicitly by all the above).
// =============================================================================

// Round-trip parity check: parse + reformat returns equivalent strings.
shouldBe(Temporal.PlainDate.from("2024-01-15").toString(), "2024-01-15", "Step 29: round-trip Date");
shouldBe(Temporal.PlainTime.from("12:30:45.123").toString(), "12:30:45.123", "Step 29: round-trip Time");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z").toString(), "2024-01-15T12:00:00Z",
    "Step 29: round-trip Instant");

// =============================================================================
// Calendar canonicalization — full union mask (calendarOf helper).
// All inputs accepted as ANY of the 6 production goals; calendar extracted or defaulted.
// =============================================================================

// Each TemporalXxxString form, with no calendar annotation → "iso8601".
shouldBe(calendarOf("2024-01-15"), "iso8601", "calendarOf: bare Date → iso8601");
shouldBe(calendarOf("2024-01-15T12:00:00"), "iso8601", "calendarOf: DateTime → iso8601");
shouldBe(calendarOf("2024-01-15T12:00:00Z"), "iso8601", "calendarOf: Instant → iso8601");
shouldBe(calendarOf("2024-01"), "iso8601", "calendarOf: YearMonth short → iso8601");
shouldBe(calendarOf("01-15"), "iso8601", "calendarOf: MonthDay short → iso8601");
shouldBe(calendarOf("12:00:00"), "iso8601", "calendarOf: Time → iso8601");
shouldBe(calendarOf("2024-01-15T12:00:00[America/New_York]"), "iso8601",
    "calendarOf: ZonedDateTime → iso8601");

// Calendar annotation extraction.
shouldBe(calendarOf("2024-01-15[u-ca=hebrew]"), "hebrew", "calendarOf: extracts hebrew");
shouldBe(calendarOf("2024-01-15T12:00:00[u-ca=islamic-civil]"), "islamic-civil",
    "calendarOf: extracts islamic-civil");

// Mixed-case calendar key: lowercased.
shouldBe(calendarOf("2024-01-15[u-ca=ISO8601]"), "iso8601", "calendarOf: case-insensitive value");

// =============================================================================
// AnnotatedTime ambiguity check (Step 4 static-semantics).
// =============================================================================

// Time-shaped inputs that are ALSO DateSpecYearMonth/DateSpecMonthDay are rejected by Time
// production but may match other goals. With Time goal alone, ambiguous inputs throw.
shouldNotThrow(() => Temporal.PlainTime.from("T12:00"),
    "ambiguity: TimeDesignator disables ambiguity check");
shouldNotThrow(() => Temporal.PlainTime.from("12:00"),
    "ambiguity: HH:MM is unambiguous (hour 12, not month 12 — colon disambiguates)");
// "1212" without TimeDesignator is ambiguous (could be 12:12 or YYYY=fail or MMDD=Dec 12).
// Since our parser tries Time first when only Time is allowed, and disambiguity is checked,
// the Time match is rejected → falls back to datetime which fails too → RangeError.
shouldThrow(() => Temporal.PlainTime.from("1212"), RangeError,
    "ambiguity: 1212 rejected (matches MMDD too)");

// =============================================================================
// Indirect entry points — equals, until, since, compare also call ToTemporal*.
// =============================================================================

{
    const a = Temporal.PlainDate.from("2024-01-15");
    shouldBe(a.equals("2024-01-15"), true, "PlainDate.equals(string)");
    shouldThrow(() => a.equals("2024-01-15T12:00:00Z"), RangeError, "PlainDate.equals: Z forbidden");
}
{
    const a = new Temporal.Instant(0n);
    shouldBe(a.equals("1970-01-01T00:00:00Z"), true, "Instant.equals(string)");
    shouldThrow(() => a.equals("1970-01-01"), RangeError, "Instant.equals: missing time/Z");
}

// =============================================================================
// Annotation deep cases (Step 4.a.ii.(1)–(2))
// =============================================================================

// Empty calendar value: [u-ca=] is invalid grammar (AnnotationValue requires ≥3 chars).
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[u-ca=]"), RangeError,
    "annotation: empty u-ca value");
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[u-ca=ab]"), RangeError,
    "annotation: too-short u-ca value");

// First non-critical calendar wins; subsequent non-critical calendars silently ignored.
shouldBe(calendarOf("2020-01-15[u-ca=hebrew][u-ca=iso8601]"), "hebrew",
    "annotation: first calendar wins, no critical flag");

// Same iso8601 calendar repeated (no critical) — accepted, single calendar tracked.
shouldBe(calendarOf("2020-01-15[u-ca=iso8601][u-ca=iso8601]"), "iso8601",
    "annotation: duplicate non-critical iso8601");

// Critical flag on non-calendar annotations — RangeError per spec.
shouldThrow(() => Temporal.PlainDate.from("2020-01-15[!unknown=value]"), RangeError,
    "annotation: critical unknown");
// Multiple unknown annotations, none critical — all ignored.
shouldNotThrow(() => Temporal.PlainDate.from("2020-01-15[foo=a][bar=b][baz=c]"),
    "annotation: multiple non-critical unknowns OK");

// =============================================================================
// Case-insensitivity: T/Z designators per Stage 4 grammar (T | t, Z | z).
// =============================================================================

shouldBe(Temporal.Instant.from("2024-01-15t12:00:00Z").epochNanoseconds, 1705320000000000000n,
    "case: lowercase t separator");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00z").epochNanoseconds, 1705320000000000000n,
    "case: lowercase z designator");
shouldBe(Temporal.Instant.from("2024-01-15t12:00:00z").epochNanoseconds, 1705320000000000000n,
    "case: both lowercase");

// =============================================================================
// Date space separator (per spec DateTimeSeparator: T | t | space).
// =============================================================================

shouldBe(Temporal.Instant.from("2024-01-15 12:00:00Z").epochNanoseconds, 1705320000000000000n,
    "DateTimeSeparator: space");
shouldNotThrow(() => Temporal.PlainDateTime.from("2024-01-15 12:00:00"), "PDT space sep");

// =============================================================================
// Calendar boundary years (Step 21 IsValidISODate).
// =============================================================================

// 1900 is NOT a leap year (divisible by 100, not by 400).
shouldThrow(() => Temporal.PlainDate.from("1900-02-29"), RangeError, "1900 not leap");
shouldNotThrow(() => Temporal.PlainDate.from("1900-02-28"), "1900 Feb 28 OK");
// 2000 IS a leap year (divisible by 400).
shouldNotThrow(() => Temporal.PlainDate.from("2000-02-29"), "2000 Feb 29 OK");
// 2100 not leap.
shouldThrow(() => Temporal.PlainDate.from("2100-02-29"), RangeError, "2100 not leap");

// =============================================================================
// Compact date forms (DateSeparator[~Extended]).
// =============================================================================

// Per spec Date production: separator presence must be CONSISTENT (all hyphens or none).
shouldNotThrow(() => Temporal.PlainDate.from("20240115"), "compact YYYYMMDD");
shouldNotThrow(() => Temporal.PlainDate.from("2024-01-15"), "extended YYYY-MM-DD");
// Mixed forms forbidden.
shouldThrow(() => Temporal.PlainDate.from("2024-0115"), RangeError, "mixed: separator-then-no");
shouldThrow(() => Temporal.PlainDate.from("202401-15"), RangeError, "mixed: no-then-separator");

// Extended year compact: ±YYYYYYMMDD (sign + 6-digit year + month + day, no separators).
shouldNotThrow(() => Temporal.PlainDate.from("+0020240115"), "compact +002024 0115");
shouldNotThrow(() => Temporal.PlainDate.from("-0019761118"), "compact -001976 1118");

// =============================================================================
// MonthDay short-form variants (DateSpecMonthDay).
// =============================================================================

// Both forms with and without leading "--".
shouldBe(Temporal.PlainMonthDay.from("--01-15").toString(), "01-15", "MD: --MM-DD");
shouldBe(Temporal.PlainMonthDay.from("--0115").toString(), "01-15", "MD: --MMDD");
shouldBe(Temporal.PlainMonthDay.from("01-15").toString(), "01-15", "MD: MM-DD");
shouldBe(Temporal.PlainMonthDay.from("0115").toString(), "01-15", "MD: MMDD");
// Feb 29 in MonthDay is valid (1972 reference year is leap).
shouldBe(Temporal.PlainMonthDay.from("--02-29").toString(), "02-29", "MD: Feb 29 OK (1972 leap)");
shouldBe(Temporal.PlainMonthDay.from("02-29").toString(), "02-29", "MD: Feb 29 OK without --");

// =============================================================================
// YearMonth short-form variants (DateSpecYearMonth).
// =============================================================================

shouldBe(Temporal.PlainYearMonth.from("2024-01").toString(), "2024-01", "YM: hyphenated");
shouldBe(Temporal.PlainYearMonth.from("202401").toString(), "2024-01", "YM: compact");
shouldBe(Temporal.PlainYearMonth.from("+002024-01").toString(), "2024-01", "YM: extended hyphen");
shouldBe(Temporal.PlainYearMonth.from("+00202401").toString(), "2024-01", "YM: extended compact");
shouldBe(Temporal.PlainYearMonth.from("-001976-11").toString(), "-001976-11", "YM: extended negative");

// =============================================================================
// UTCOffset edge cases.
// =============================================================================

// Sub-minute precision: ±HH:MM:SS or ±HH:MM:SS.fffffffff.
shouldNotThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:30:15"), "offset HH:MM:SS");
shouldNotThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:30:15.5"), "offset HH:MM:SS.f");
shouldNotThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:30:15.123456789"), "offset HH:MM:SS.fffffffff");
// Compact offset forms.
shouldNotThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+0530"), "compact offset ±HHMM");
shouldNotThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+053015"), "compact offset ±HHMMSS");
// Mixed extended/compact in offset rejected.
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:3015"), RangeError, "offset mixed");

// =============================================================================
// Round-trip from Temporal types via toString().
// =============================================================================

const inputs = [
    ["Instant",       "2024-01-15T12:30:45.123456789Z"],
    ["PlainDate",     "2024-01-15"],
    ["PlainDateTime", "2024-01-15T12:30:45.123456789"],
    ["PlainTime",     "12:30:45.123456789"],
    ["PlainYearMonth","2024-01"],
    ["PlainMonthDay", "01-15"],
];
for (const [name, str] of inputs)
    shouldBe(Temporal[name].from(str).toString(), str, `round-trip ${name} "${str}"`);
