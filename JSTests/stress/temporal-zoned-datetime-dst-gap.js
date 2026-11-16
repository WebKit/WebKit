//@ requireOptions("--useTemporal=1")

// temporal-zdt-dst-gap.js — DST spring-forward gap disambiguation stress test

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function shouldThrow(fn, msg) {
    try { fn(); throw new Error(`${msg}: should have thrown`); }
    catch (e) { if (e.message.startsWith(msg)) throw e; }
}

// America/New_York spring-forward 2024-03-10: 02:00-02:59 doesn't exist
// Clocks jump from 01:59 EST → 03:00 EDT

// Compatible for gap: per spec, "compatible" acts like "later" for gaps
// (jump forward past the gap → post-transition time)
{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T02:30[America/New_York]");
    shouldBe(zdt.hour, 3, "Compatible gap jumps forward past gap");
    shouldBe(zdt.offset, "-04:00", "Compatible gap uses EDT (post-transition)");
}

// "earlier" for gap: use post-transition offset → maps to pre-gap time
// Spec: naiveNs - offsetAfter → 02:30 - (-4h) = 06:30Z → 01:30 EST
{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T02:30[America/New_York]", { disambiguation: "earlier" });
    shouldBe(zdt.hour, 1, "Earlier gap result hour");
    shouldBe(zdt.offset, "-05:00", "Earlier gap offset");
}

// "later" for gap: use pre-transition offset → maps to post-gap time
// Spec: naiveNs - offsetBefore → 02:30 - (-5h) = 07:30Z → 03:30 EDT
{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T02:30[America/New_York]", { disambiguation: "later" });
    shouldBe(zdt.hour, 3, "Later gap result hour");
    shouldBe(zdt.offset, "-04:00", "Later gap offset");
}

{
    shouldThrow(
        () => Temporal.ZonedDateTime.from("2024-03-10T02:30[America/New_York]", { disambiguation: "reject" }),
        "Reject gap should throw"
    );
}

{
    const before = Temporal.ZonedDateTime.from("2024-03-10T01:59[America/New_York]");
    shouldBe(before.offset, "-05:00", "1:59 AM is still EST");
    shouldBe(before.hour, 1, "hour before gap");

    const after = Temporal.ZonedDateTime.from("2024-03-10T03:00[America/New_York]");
    shouldBe(after.offset, "-04:00", "3:00 AM is EDT");
    shouldBe(after.hour, 3, "hour after gap");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T02:30[UTC]");
    shouldBe(zdt.hour, 2, "UTC has no gap");
    shouldBe(zdt.offset, "+00:00", "UTC offset");
}
