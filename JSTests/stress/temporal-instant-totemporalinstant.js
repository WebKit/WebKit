//@ requireOptions("--useTemporal=1")

// Tests ToTemporalInstant — https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
// Exercised via Temporal.Instant.from and the indirect entry points (equals/until/since/compare).

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

// Object input: existing Instant / ZonedDateTime returns a NEW Instant with the same epochNs.

{
    const original = new Temporal.Instant(1234567890n);
    const result = Temporal.Instant.from(original);
    shouldBe(result.epochNanoseconds, 1234567890n, "Instant.from(Instant): epochNs preserved");
    if (result === original)
        throw new Error("Instant.from(Instant): must return a NEW instance, not same identity");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-06-15T12:00:00[UTC]");
    shouldBe(Temporal.Instant.from(zdt).epochNanoseconds, zdt.epochNanoseconds, "Instant.from(ZonedDateTime)");
}

// Object input without a slot → ToPrimitive(item, STRING) coercion.

shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return 42; } }), TypeError, "→Number");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return -7.5; } }), TypeError, "→negative Number");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return true; } }), TypeError, "→Boolean true");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return false; } }), TypeError, "→Boolean false");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return 1234567890n; } }), TypeError, "→BigInt");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return Symbol("x"); } }), TypeError, "→Symbol");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return null; } }), TypeError, "→null");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return undefined; } }), TypeError, "→undefined");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return { x: 1 }; } }), TypeError, "→Object");
shouldThrow(() => Temporal.Instant.from({ [Symbol.toPrimitive]() { return []; } }), TypeError, "→Array");

{
    const sentinel = new Error("custom");
    let caught;
    try { Temporal.Instant.from({ [Symbol.toPrimitive]() { throw sentinel; } }); } catch (e) { caught = e; }
    shouldBe(caught, sentinel, "Symbol.toPrimitive throw propagates");
}

{
    let receivedHint;
    Temporal.Instant.from({ [Symbol.toPrimitive](hint) { receivedHint = hint; return "2024-01-15T12:00:00Z"; } });
    shouldBe(receivedHint, "string", "Symbol.toPrimitive hint is 'string'");
}

// No Symbol.toPrimitive → toString first, valueOf second.
shouldBe(Temporal.Instant.from({
    toString() { return "2024-01-15T12:00:00Z"; },
    valueOf() { return 999; },
}).epochNanoseconds, 1705320000000000000n, "toString String wins over valueOf");

shouldBe(Temporal.Instant.from({
    toString() { return { x: 1 }; },
    valueOf() { return "2024-01-15T12:00:00Z"; },
}).epochNanoseconds, 1705320000000000000n, "valueOf used when toString returns Object");

shouldThrow(() => Temporal.Instant.from({ toString() { return {}; }, valueOf() { return {}; } }),
    TypeError, "Both toString and valueOf return Object");
shouldThrow(() => Temporal.Instant.from({ toString() { return 42; }, valueOf() { return 99; } }),
    TypeError, "Both return non-String primitives");

// Bare non-String primitives → TypeError.

shouldThrow(() => Temporal.Instant.from(42), TypeError, "bare Number");
shouldThrow(() => Temporal.Instant.from(0), TypeError, "bare Number 0");
shouldThrow(() => Temporal.Instant.from(NaN), TypeError, "bare NaN");
shouldThrow(() => Temporal.Instant.from(Infinity), TypeError, "bare Infinity");
shouldThrow(() => Temporal.Instant.from(true), TypeError, "bare Boolean true");
shouldThrow(() => Temporal.Instant.from(false), TypeError, "bare Boolean false");
shouldThrow(() => Temporal.Instant.from(1234567890n), TypeError, "bare BigInt");
shouldThrow(() => Temporal.Instant.from(null), TypeError, "null");
shouldThrow(() => Temporal.Instant.from(undefined), TypeError, "undefined");
shouldThrow(() => Temporal.Instant.from(Symbol("x")), TypeError, "bare Symbol");

// Valid TemporalInstantString formats.

shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z").epochNanoseconds, 1705320000000000000n, "T separator + Z");
shouldBe(Temporal.Instant.from("2024-01-15 12:00:00Z").epochNanoseconds, 1705320000000000000n, "space separator + Z");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:30").epochNanoseconds, 1705300200000000000n, "+05:30");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00-14:00").epochNanoseconds, 1705370400000000000n, "-14:00 max");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+14:00").epochNanoseconds, 1705269600000000000n, "+14:00 max");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00.123456789Z").epochNanoseconds, 1705320000123456789n, "9 frac digits");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00.5Z").epochNanoseconds, 1705320000500000000n, "1 frac digit");

// Invalid string / missing offset → RangeError.

shouldThrow(() => Temporal.Instant.from(""), RangeError, "empty");
shouldThrow(() => Temporal.Instant.from("garbage"), RangeError, "garbage");
shouldThrow(() => Temporal.Instant.from("2024-01-15"), RangeError, "Date only");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00"), RangeError, "no offset/Z");
shouldThrow(() => Temporal.Instant.from("2024-13-15T12:00:00Z"), RangeError, "month=13");
shouldThrow(() => Temporal.Instant.from("2024-00-15T12:00:00Z"), RangeError, "month=0");
shouldThrow(() => Temporal.Instant.from("2024-01-32T12:00:00Z"), RangeError, "day=32");
shouldThrow(() => Temporal.Instant.from("2024-02-30T12:00:00Z"), RangeError, "Feb 30");
shouldThrow(() => Temporal.Instant.from("2024-01-15T25:00:00Z"), RangeError, "hour=25");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:60:00Z"), RangeError, "minute=60");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00Z extra"), RangeError, "trailing junk");

// Offset format variants.

shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+00:00").epochNanoseconds,
    Temporal.Instant.from("2024-01-15T12:00:00Z").epochNanoseconds, "+00:00 == Z");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00-00:00").epochNanoseconds,
    Temporal.Instant.from("2024-01-15T12:00:00Z").epochNanoseconds, "-00:00 == Z");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05").epochNanoseconds, 1705302000000000000n, "bare ±HH");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:30:15").epochNanoseconds, 1705300185000000000n, "offset with seconds");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:30:15.123456789").epochNanoseconds, 1705300184876543211n, "fractional offset");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+5:30"), RangeError, "+5:30");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:3"), RangeError, "+05:3");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+25:00"), RangeError, "+25:00");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00+05:60"), RangeError, "+05:60");

// Epoch range (IsValidEpochNanoseconds): max ±8.64e21 ns.

shouldBe(Temporal.Instant.from("+275760-09-13T00:00:00Z").epochNanoseconds, 8640000000000000000000n, "max year");
shouldBe(Temporal.Instant.from("-271821-04-20T00:00:00Z").epochNanoseconds, -8640000000000000000000n, "min year");
shouldThrow(() => Temporal.Instant.from("+275760-09-14T00:00:00Z"), RangeError, "+1 day past max");
shouldThrow(() => Temporal.Instant.from("-271821-04-19T00:00:00Z"), RangeError, "-1 day before min");
shouldThrow(() => Temporal.Instant.from("+999999-01-01T00:00:00Z"), RangeError, "far future");
shouldThrow(() => Temporal.Instant.from("-999999-01-01T00:00:00Z"), RangeError, "far past");

shouldBe(Temporal.Instant.from("1969-12-31T23:59:59Z").epochNanoseconds, -1000000000n, "1 second before epoch");
shouldBe(Temporal.Instant.from("1969-12-31T23:59:59.999999999Z").epochNanoseconds, -1n, "1 ns before epoch");
shouldBe(Temporal.Instant.from("1969-12-31T23:59:59.999999999+00:00").epochNanoseconds, -1n, "negative epoch via +00:00");

// Round-trip: parsed string equals constructor with the same epochNs.

{
    const fromString = Temporal.Instant.from("2024-01-15T12:00:00Z");
    const fromCtor = new Temporal.Instant(1705320000000000000n);
    shouldBe(fromString.epochNanoseconds, fromCtor.epochNanoseconds, "from(string) == new Instant(ns)");
}

// RFC 9557 annotations: u-ca is a recognized key so the critical flag is a no-op;
// bracket TZ is ignored when offset/Z is present; bracket TZ alone (no offset/Z) is rejected.

shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z[u-ca=iso8601]").epochNanoseconds, 1705320000000000000n, "[u-ca=iso8601]");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z[u-ca=hebrew]").epochNanoseconds, 1705320000000000000n, "[u-ca=hebrew]");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z[!u-ca=hebrew]").epochNanoseconds, 1705320000000000000n, "[!u-ca=hebrew]");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00Z[America/New_York]").epochNanoseconds, 1705320000000000000n, "Z + named TZ");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00+05:00[America/New_York]").epochNanoseconds, 1705302000000000000n, "offset + named TZ");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00-05:00[!America/New_York]").epochNanoseconds, 1705338000000000000n, "[!TZ] + offset");
shouldThrow(() => Temporal.Instant.from("2024-01-15T12:00:00[America/New_York]"), RangeError, "TZ alone (no offset/Z)");

// Fractional digit sweep: all forms encode 0.1 sec = 1e8 ns regardless of trailing zeros; >9 digits rejected.
for (let digits = 1; digits <= 9; ++digits) {
    const frac = "1".padEnd(digits, "0");
    shouldBe(Temporal.Instant.from(`1970-01-01T00:00:00.${frac}Z`).epochNanoseconds, 100000000n, `${digits} frac digit(s)`);
}
shouldThrow(() => Temporal.Instant.from("1970-01-01T00:00:00.1234567890Z"), RangeError, "10 frac digits");

// Time grammar: 24:00 rejected (Hour ::= 00..23). Leap second :60 clamped to :59 (same instant).
shouldThrow(() => Temporal.Instant.from("2024-01-15T24:00:00Z"), RangeError, "24:00:00");
shouldBe(Temporal.Instant.from("2024-06-30T23:59:60Z").epochNanoseconds, 1719791999000000000n, ":60 → :59");

// T/Z designators are case-insensitive per spec grammar.
shouldBe(Temporal.Instant.from("2024-01-15t12:00:00Z").epochNanoseconds, 1705320000000000000n, "lowercase t");
shouldBe(Temporal.Instant.from("2024-01-15T12:00:00z").epochNanoseconds, 1705320000000000000n, "lowercase z");

// Indirect entry points: equals / until / since / compare all call ToTemporalInstant on the other operand.
{
    const a = new Temporal.Instant(1000n);
    shouldBe(a.equals("1970-01-01T00:00:00.000001Z"), true, "equals(string)");
    shouldBe(a.until("1970-01-01T00:00:00.000002Z").total('nanoseconds'), 1000, "until(string)");
    shouldBe(a.since("1970-01-01T00:00:00Z").total('nanoseconds'), 1000, "since(string)");
    shouldBe(Temporal.Instant.compare("1970-01-01T00:00:00Z", "1970-01-01T00:00:00.000001Z"), -1, "compare strings");
    shouldThrow(() => a.equals({ [Symbol.toPrimitive]() { return 42; } }), TypeError, "equals(→Number)");
    shouldThrow(() => a.until({ [Symbol.toPrimitive]() { return 42; } }), TypeError, "until(→Number)");
    shouldThrow(() => Temporal.Instant.compare({ [Symbol.toPrimitive]() { return 42; } }, a), TypeError, "compare(→Number)");
}

// Result class identity.
{
    const r = Temporal.Instant.from("2024-01-15T12:00:00Z");
    if (r.constructor !== Temporal.Instant)
        throw new Error("constructor must be Temporal.Instant");
    if (!(r instanceof Temporal.Instant))
        throw new Error("instanceof Temporal.Instant must be true");
}
