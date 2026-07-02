//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Verify ZDT rounding against V8/temporal_rs by checking JSC matches expected values.
//
// Root bug: roundZonedDateTimeSubDay used quantity from LOCAL MIDNIGHT for all units,
// but spec RoundTime uses sub-unit quantities:
//   hour:        from midnight (all components)
//   minute:      sub-hour (minute×60 + sub-minute ns)
//   second:      sub-minute (second + sub-second ns)
//   millisecond: sub-second (ms + sub-ms ns)
//   microsecond: sub-microsecond (us + ns)
//   nanosecond:  nanosecond only
//
// This affects halfEven/halfFloor/halfCeil when the quotient parity differs
// between the from-midnight value and the sub-unit value.

// ── Helper: round ZDT and check ─────────────────────────────────────────
function checkRound(zdtStr, unit, increment, mode, expectedField, expectedValue) {
    const zdt = Temporal.ZonedDateTime.from(zdtStr);
    const r = zdt.round({ smallestUnit: unit, roundingIncrement: increment, roundingMode: mode });
    if (r[expectedField] !== expectedValue)
        throw new Error(`${zdtStr} round(${unit},${increment},${mode}): expected ${expectedField}=${expectedValue} got ${r[expectedField]}`);
}

// ── Minute rounding: halfEven ─────────────────────────────────────────────
// Sub-hour quotient: 26*60e9/240e9 = 6.5 → r1=6 (even) → 11:24
checkRound("1969-12-31T11:26:00-12:34[-12:34]", "minutes", 4, "halfEven", "minute", 24);
checkRound("2020-01-01T11:26:00+00:00[+00:00]", "minutes", 4, "halfEven", "minute", 24);
checkRound("2020-01-01T11:26:00+01:23[+01:23]", "minutes", 4, "halfEven", "minute", 24);

// All 15 half-increment points in an hour (4-min increment, halfEven)
// q = minute/4 — even q → round to q*4, odd q → round to (q+1)*4
for (const [min, expected] of [
    [2,0],[6,8],[10,8],[14,16],[18,16],[22,24],[26,24],
    [30,32],[34,32],[38,40],[42,40],[46,48],[50,48],[54,56],[58,56]
]) {
    checkRound(`2020-01-01T00:${String(min).padStart(2,"0")}:00+00:00[+00:00]`, "minutes", 4, "halfEven", "minute", expected);
}

// ── Minute rounding: halfCeil and halfFloor ─────────────────────────────
// halfCeil ties go to ceiling (r2), halfFloor ties go to floor (r1)
// 11:26 tie: r1=11:24, r2=11:28
checkRound("2020-01-01T11:26:00+00:00[+00:00]", "minutes", 4, "halfCeil",  "minute", 28);
checkRound("2020-01-01T11:26:00+00:00[+00:00]", "minutes", 4, "halfFloor", "minute", 24);
checkRound("2020-01-01T11:26:00+00:00[+00:00]", "minutes", 4, "halfTrunc",  "minute", 24); // toward zero
checkRound("2020-01-01T11:26:00+00:00[+00:00]", "minutes", 4, "halfExpand", "minute", 28); // away from zero

// ── Minute: various increments at half points ──────────────────────────
// 3-minute: 01:30:00 → 1.5 minutes. sub-hour=90e9/180e9=0.5, r1=0(even) → :00
checkRound("2020-01-01T00:01:30+00:00[+00:00]", "minutes", 3, "halfEven", "minute", 0);
// 5-minute: 02:30:00 → sub-hour=150e9/300e9=0.5, r1=0(even) → :00
checkRound("2020-01-01T00:02:30+00:00[+00:00]", "minutes", 5, "halfEven", "minute", 0);
// 6-minute: 03:00:00 → sub-hour=180e9/360e9=0.5, r1=0(even) → :00
checkRound("2020-01-01T00:03:00+00:00[+00:00]", "minutes", 6, "halfEven", "minute", 0);
// 15-minute: 07:30:00 → 450e9/900e9=0.5, r1=0(even) → :00
checkRound("2020-01-01T00:07:30+00:00[+00:00]", "minutes", 15, "halfEven", "minute", 0);
// 30-minute: 15:00:00 → sub-hour=900e9/1800e9=0.5, r1=0(even) → :00
checkRound("2020-01-01T00:15:00+00:00[+00:00]", "minutes", 30, "halfEven", "minute", 0);

// ── Second rounding: halfEven ─────────────────────────────────────────
// sub-minute quantity = second×1e9 + sub-second
// 00:00:30.000 with 30s increment → sub-minute=30e9/30e9=1.0 exact → 30s
checkRound("2020-01-01T00:00:30+00:00[+00:00]", "seconds", 30, "halfEven", "second", 30);
// 00:00:15 with 30s increment → sub-minute=15e9/30e9=0.5, r1=0(even) → 0s
checkRound("2020-01-01T00:00:15+00:00[+00:00]", "seconds", 30, "halfEven", "second", 0);
// 00:00:30 with 1s increment → exact, no rounding
checkRound("2020-01-01T00:00:30+00:00[+00:00]", "seconds", 1, "halfEven", "second", 30);
// 00:01:15 with 30s halfEven: quantity=second=15e9, 15/30=0.5, r1=0(0,even) → second=0
// baseOffset = 1*60e9. roundedLocalNs = 60e9 → minute=1, second=0
{
    const r = Temporal.ZonedDateTime.from("2020-01-01T00:01:15+00:00[+00:00]")
        .round({ smallestUnit: "seconds", roundingIncrement: 30, roundingMode: "halfEven" });
    shouldBe(r.minute, 1);
    shouldBe(r.second, 0);
}
// 00:00:45 with 30s halfEven: quantity=45e9, 45/30=1.5, r1=30e9(1,odd) r2=60e9(2,even) → overflow
// baseOffset=0. roundedLocalNs=60e9 → minute=1, second=0
{
    const r = Temporal.ZonedDateTime.from("2020-01-01T00:00:45+00:00[+00:00]")
        .round({ smallestUnit: "seconds", roundingIncrement: 30, roundingMode: "halfEven" });
    shouldBe(r.minute, 1);
    shouldBe(r.second, 0);
}

// ── Millisecond rounding: halfEven ────────────────────────────────────
// sub-second quantity = ms×1e6 + sub-ms
// 00:00:00.250 with 500ms increment → 250e6/500e6=0.5, r1=0(0,even) → 0ms
checkRound("2020-01-01T00:00:00.250+00:00[+00:00]", "milliseconds", 500, "halfEven", "millisecond", 0);
// 00:00:00.500 with 500ms → 500e6/500e6=1.0 exact → 500ms
checkRound("2020-01-01T00:00:00.500+00:00[+00:00]", "milliseconds", 500, "halfEven", "millisecond", 500);
// 00:00:00.100 with 200ms → 100e6/200e6=0.5, r1=0(0,even) → 0ms
checkRound("2020-01-01T00:00:00.100+00:00[+00:00]", "milliseconds", 200, "halfEven", "millisecond", 0);
// 00:00:00.750 with 500ms → 750e6/500e6=1.5, r1=500e6(1,odd) r2=1000e6(2,even) → 2*500=1000ms → 1s overflow
{
    const r = Temporal.ZonedDateTime.from("2020-01-01T00:00:00.750+00:00[+00:00]")
        .round({ smallestUnit: "milliseconds", roundingIncrement: 500, roundingMode: "halfEven" });
    shouldBe(r.second, 1);
    shouldBe(r.millisecond, 0);
}

// ── Microsecond rounding: halfEven ────────────────────────────────────
// sub-ms quantity = us×1000 + ns
// 00:00:00.000500 (500µs) with 500µs increment → exact → 500µs
checkRound("2020-01-01T00:00:00.0005+00:00[+00:00]", "microseconds", 500, "halfEven", "microsecond", 500);
// 00:00:00.000250 (250µs) with 500µs increment → 250000/500000=0.5, r1=0(0,even) → 0µs
checkRound("2020-01-01T00:00:00.00025+00:00[+00:00]", "microseconds", 500, "halfEven", "microsecond", 0);

// ── Nanosecond rounding: halfEven ─────────────────────────────────────
// quantity = nanosecond only (independent of higher units)
// ns=500 with 500ns increment → exact → 500ns; ns=250 with 500ns → 0.5 → r1=0(0,even) → 0ns
checkRound("2020-01-01T00:00:00.0000005+00:00[+00:00]", "nanoseconds", 500, "halfEven", "nanosecond", 500);
checkRound("2020-01-01T00:00:00.00000025+00:00[+00:00]", "nanoseconds", 500, "halfEven", "nanosecond", 0);

// ── Hour rounding: halfEven (from-midnight, unchanged behavior) ────────
// 01:30 → 1.5h, r1=1(odd) r2=2(even) → 2h
checkRound("2020-01-01T01:30:00+00:00[+00:00]", "hours", 1, "halfEven", "hour", 2);
// 12:30 → 12.5h, r1=12(even) r2=13(odd) → 12h
checkRound("2020-01-01T12:30:00+00:00[+00:00]", "hours", 1, "halfEven", "hour", 12);
// 2h with 2h increment: 1:00 → 1/2=0.5, r1=0(even) → 0h
checkRound("2020-01-01T01:00:00+00:00[+00:00]", "hours", 2, "halfEven", "hour", 0);

// ── Day overflow on round up ─────────────────────────────────────────
// 23:58 ceiled to 4-min → 24:00 = midnight next day
{
    const r = Temporal.ZonedDateTime.from("2020-01-01T23:58:00+00:00[+00:00]")
        .round({ smallestUnit: "minutes", roundingIncrement: 4, roundingMode: "ceil" });
    shouldBe(r.day, 2);
    shouldBe(r.hour, 0);
    shouldBe(r.minute, 0);
}

// ── Large epoch ns values (from interestingEpochNs) ──────────────────
// These should round correctly regardless of the epoch magnitude.
// epoch=2^64 with offset -12:34: local time computed from epoch, must round correctly.
for (const epochNs of [0n, 1n, -1n, 2n**32n, -(2n**32n), 1_000_000_000_000_000_000n]) {
    const zdt = new Temporal.ZonedDateTime(epochNs, "+00:00");
    // Should not throw; result must be on a 4-minute boundary
    const r = zdt.round({ smallestUnit: "minutes", roundingIncrement: 4, roundingMode: "trunc" });
    shouldBe(r.minute % 4, 0);
    shouldBe(r.second, 0);
    shouldBe(r.millisecond, 0);
    shouldBe(r.microsecond, 0);
    shouldBe(r.nanosecond, 0);
}

// ── Consistency: result must always land on increment boundary ────────
const testZDTs = [
    "2020-01-01T11:26:37.123456789+00:00[+00:00]",
    "1969-12-31T11:26:00-12:34[-12:34]",
    "2020-06-15T23:59:59.999999999+00:00[+00:00]",
];
for (const s of testZDTs) {
    for (const [unit, inc, field] of [
        ["minutes", 4, "minute"], ["minutes", 15, "minute"], ["minutes", 30, "minute"],
        ["seconds", 30, "second"], ["milliseconds", 500, "millisecond"],
    ]) {
        for (const mode of ["ceil","floor","trunc","expand","halfEven","halfCeil","halfFloor","halfExpand","halfTrunc"]) {
            const r = Temporal.ZonedDateTime.from(s).round({ smallestUnit: unit, roundingIncrement: inc, roundingMode: mode });
            if (r[field] % inc !== 0)
                throw new Error(`${s} ${unit}/${inc}/${mode}: ${field}=${r[field]} not on boundary`);
        }
    }
}
