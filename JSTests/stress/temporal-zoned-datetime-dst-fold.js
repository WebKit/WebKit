//@ requireOptions("--useTemporal=1")

// temporal-zdt-dst-fold.js — DST fall-back fold disambiguation stress test
// Regression test for T13 Compatible fold bug (returned LATER instead of EARLIER)
//
// America/New_York fall-back 2024-11-03: 01:00-01:59 occurs twice
//   01:30 EDT (UTC-4) = 05:30 UTC  (earlier)
//   01:30 EST (UTC-5) = 06:30 UTC  (later)

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function shouldThrow(fn, msg) {
    try { fn(); throw new Error(`${msg}: should have thrown`); }
    catch (e) { if (e.message.startsWith(msg)) throw e; }
}

// Compatible (default) picks EARLIER instant during fold
{
    const zdt = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]");
    shouldBe(zdt.offset, "-04:00", "Compatible fold should pick EDT (earlier)");
    shouldBe(zdt.hour, 1, "hour");
    shouldBe(zdt.minute, 30, "minute");
}

// Explicit "compatible" — same as default
{
    const zdt = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "compatible" });
    shouldBe(zdt.offset, "-04:00", "Explicit compatible fold should pick EDT");
}

// "earlier" picks first occurrence (EDT)
{
    const zdt = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "earlier" });
    shouldBe(zdt.offset, "-04:00", "Earlier fold should pick EDT");
}

// "later" picks second occurrence (EST)
{
    const zdt = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "later" });
    shouldBe(zdt.offset, "-05:00", "Later fold should pick EST");
}

// "reject" throws for ambiguous time
{
    shouldThrow(
        () => Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "reject" }),
        "Reject fold should throw"
    );
}

// Verify epoch difference between earlier and later is exactly 1 hour
{
    const earlier = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "earlier" });
    const later = Temporal.ZonedDateTime.from("2024-11-03T01:30[America/New_York]", { disambiguation: "later" });
    const diffNs = later.epochNanoseconds - earlier.epochNanoseconds;
    shouldBe(diffNs, 3600000000000n, "Earlier and later should be 1h apart");
}

// Australia/Lord_Howe: 30-minute DST shift (non-standard)
// Fall-back: +11:00 → +10:30 (clocks go back 30 min)
// 2024-04-07 at 02:00 LHDT → 01:30 LHST
{
    const earlier = Temporal.ZonedDateTime.from("2024-04-07T01:45[Australia/Lord_Howe]", { disambiguation: "earlier" });
    const later = Temporal.ZonedDateTime.from("2024-04-07T01:45[Australia/Lord_Howe]", { disambiguation: "later" });
    const diffNs = later.epochNanoseconds - earlier.epochNanoseconds;
    shouldBe(diffNs, 1800000000000n, "Lord Howe fold should be 30min apart");
}

// Non-fold time should return same result for all disambiguations
{
    const c = Temporal.ZonedDateTime.from("2024-06-15T12:00[America/New_York]", { disambiguation: "compatible" });
    const e = Temporal.ZonedDateTime.from("2024-06-15T12:00[America/New_York]", { disambiguation: "earlier" });
    const l = Temporal.ZonedDateTime.from("2024-06-15T12:00[America/New_York]", { disambiguation: "later" });
    shouldBe(c.epochNanoseconds, e.epochNanoseconds, "Non-fold: compatible == earlier");
    shouldBe(c.epochNanoseconds, l.epochNanoseconds, "Non-fold: compatible == later");
}
