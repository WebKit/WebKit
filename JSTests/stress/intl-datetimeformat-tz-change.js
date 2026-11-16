//@ runDefault("--useDollarVM=1")
//@ skip if $hostOS == "playstation"

// Subsequent constructions must observe a host TZ change. Driven across real
// setTimeout boundaries so the change goes through the production VMEntryScope
// service path, where the impl cache is invalidated. Chained setTimeout, because
// async rejections get swallowed in jsc.cpp.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected "${want}", got "${got}"`);
}

const tzs = ["Europe/Paris", "Asia/Tokyo", "America/Los_Angeles", "UTC", "Australia/Sydney"];
const epoch = 0; // 1970-01-01T00:00:00Z lands on different calendar days east vs west of UTC.
const formattedByTz = new Map();

function runCycle(i)
{
    if (i >= tzs.length) {
        afterCycles();
        return;
    }
    const tz = tzs[i];
    if (!$vm.setHostTimeZone(tz))
        throw new Error(`Failed to set host time zone to ${tz}`);
    setTimeout(() => {
        // Cacheable no-arg and string-locale shapes, and a distinct locale, all see the new host.
        expect(`[${tz}] no-arg ctor sees host`, new Intl.DateTimeFormat().resolvedOptions().timeZone, tz);
        expect(`[${tz}] string locale sees host`, new Intl.DateTimeFormat("en-US").resolvedOptions().timeZone, tz);
        expect(`[${tz}] distinct locale sees host`, new Intl.DateTimeFormat("ja").resolvedOptions().timeZone, tz);
        // Explicit timeZone option always wins over the host.
        expect(`[${tz}] explicit timeZone wins`,
            new Intl.DateTimeFormat("en-US", { timeZone: "Asia/Kolkata" }).resolvedOptions().timeZone, "Asia/Kolkata");
        formattedByTz.set(tz, new Intl.DateTimeFormat("en-US").format(epoch));
        runCycle(i + 1);
    }, 0);
}

function afterCycles()
{
    // Distinct host TZs must format the same epoch differently.
    if (new Set(formattedByTz.values()).size < 2)
        throw new Error(`format() output never differed across host TZs: ${JSON.stringify([...formattedByTz.entries()])}`);

    // Non-TZ resolved-options fields are unaffected by TZ changes.
    if (!$vm.setHostTimeZone("UTC"))
        throw new Error("Failed to set host time zone to UTC");
    setTimeout(() => {
        const r = new Intl.DateTimeFormat("en-US").resolvedOptions();
        expect("locale stable across TZ changes", r.locale, "en-US");
        expect("calendar stable across TZ changes", r.calendar, "gregory");
        expect("numberingSystem stable across TZ changes", r.numberingSystem, "latn");
    }, 0);
}

runCycle(0);
