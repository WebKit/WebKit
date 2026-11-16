//@ requireOptions("--useTemporal=1")
// Stress test for createTemporalFormatter caching:
// The formatter for each TemporalFieldKind is computed once and cached.
// Verify that repeated calls and mixed-kind calls produce correct, consistent output.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

// Fixed epoch for deterministic output regardless of timezone.
const t = new Date(Date.UTC(2024, 0, 15, 10, 30, 45)); // 2024-01-15T10:30:45Z

const plainDate = new Temporal.PlainDate(2024, 1, 15);
const plainTime = new Temporal.PlainTime(10, 30, 45);
const plainDateTime = new Temporal.PlainDateTime(2024, 1, 15, 10, 30, 45);
const plainYM = new Temporal.PlainYearMonth(2024, 1);
const plainMD = new Temporal.PlainMonthDay(1, 15);
const instant = new Temporal.Instant(BigInt(t.getTime()) * 1_000_000n);

const fmt = new Intl.DateTimeFormat("en-US", {
    year: "numeric", month: "short", day: "numeric",
    hour: "numeric", minute: "2-digit", second: "2-digit",
    timeZone: "UTC",
    calendar: "iso8601"
});

// 1. Repeated calls on same kind must produce identical output.
{
    const N = 100;
    const first = fmt.format(plainDate);
    for (let i = 0; i < N; i++)
        shouldBe(fmt.format(plainDate), first, `plainDate iteration ${i}`);

    const firstTime = fmt.format(plainTime);
    for (let i = 0; i < N; i++)
        shouldBe(fmt.format(plainTime), firstTime, `plainTime iteration ${i}`);

    const firstDT = fmt.format(plainDateTime);
    for (let i = 0; i < N; i++)
        shouldBe(fmt.format(plainDateTime), firstDT, `plainDateTime iteration ${i}`);
}

// 2. Different kinds on same formatter don't interfere — interleave them.
{
    const r1 = fmt.format(plainDate);
    const r2 = fmt.format(plainTime);
    const r3 = fmt.format(plainDateTime);
    const r4 = fmt.format(plainYM);
    const r5 = fmt.format(plainMD);

    for (let i = 0; i < 50; i++) {
        shouldBe(fmt.format(plainDate), r1, `interleaved plainDate ${i}`);
        shouldBe(fmt.format(plainTime), r2, `interleaved plainTime ${i}`);
        shouldBe(fmt.format(plainDateTime), r3, `interleaved plainDateTime ${i}`);
        shouldBe(fmt.format(plainYM), r4, `interleaved plainYearMonth ${i}`);
        shouldBe(fmt.format(plainMD), r5, `interleaved plainMonthDay ${i}`);
    }
}

// 3. Plain types must use GMT (not the formatter's timezone), so the date
//    fields come from the Temporal value itself, not from epoch conversion.
{
    // A formatter with a non-UTC timezone — plain types must still show the
    // Temporal value's fields unchanged (they don't do timezone conversion).
    const fmtLA = new Intl.DateTimeFormat("en-US", {
        year: "numeric", month: "numeric", day: "numeric",
        hour: "numeric", minute: "2-digit",
        timeZone: "America/Los_Angeles"
    });
    // PlainDate 2024-01-15 must format as Jan 15, regardless of LA offset.
    const pdResult = fmtLA.format(plainDate);
    shouldBe(pdResult.includes("1/15/2024") || pdResult.includes("15"), true,
        "PlainDate should show Jan 15 regardless of LA timezone");

    // Instant uses the formatter's timezone (LA), so it could shift the day.
    const instResult = fmtLA.format(instant); // 10:30 UTC = 02:30 LA → Jan 15 still
    // Just verify it formats without throwing and is consistent.
    shouldBe(fmtLA.format(instant), instResult, "Instant result stable");

    // Repeated calls still match.
    for (let i = 0; i < 20; i++) {
        shouldBe(fmtLA.format(plainDate), pdResult, `LA plainDate ${i}`);
        shouldBe(fmtLA.format(instant), instResult, `LA instant ${i}`);
    }
}

// 4. formatToParts and format share the same cache — verify consistency.
{
    const partsDate = fmt.formatToParts(plainDate);
    const fmtDate = fmt.format(plainDate);
    const reconstructed = partsDate.map(p => p.value).join("");
    shouldBe(reconstructed, fmtDate, "formatToParts reconstructs same string as format");

    for (let i = 0; i < 30; i++) {
        shouldBe(fmt.format(plainDate), fmtDate, `format after formatToParts ${i}`);
        shouldBe(fmt.formatToParts(plainDate).map(p => p.value).join(""), fmtDate,
            `formatToParts after format ${i}`);
    }
}

// 5. dateStyle/timeStyle formatters cache correctly.
{
    const fmtDS = new Intl.DateTimeFormat("en-US", { dateStyle: "short", timeZone: "UTC" });
    const fmtTS = new Intl.DateTimeFormat("en-US", { timeStyle: "short", timeZone: "UTC" });

    const dsDate = fmtDS.format(plainDate);
    const tsTime = fmtTS.format(plainTime);
    const tsDT = fmtTS.format(plainDateTime);

    for (let i = 0; i < 50; i++) {
        shouldBe(fmtDS.format(plainDate), dsDate, `dateStyle plainDate ${i}`);
        shouldBe(fmtTS.format(plainTime), tsTime, `timeStyle plainTime ${i}`);
        shouldBe(fmtTS.format(plainDateTime), tsDT, `timeStyle plainDateTime ${i}`);
    }

    // dateStyle + PlainTime must throw (no overlap).
    let threw = false;
    try { fmtDS.format(plainTime); } catch (e) { threw = true; }
    shouldBe(threw, true, "dateStyle + PlainTime must throw TypeError");
}
