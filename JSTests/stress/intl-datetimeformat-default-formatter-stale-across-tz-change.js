//@ skip
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=318362
// Skipped: exposes a known, deferred bug. Distinct from the DateInstanceData
// staleness covered by intl-datetimeformat-stale-data-*-across-tz-change.js:
// there the stale state is a held Date instance's cached GregorianDateTime,
// while here even a freshly created Date is affected. The per-global default
// formatters (JSGlobalObject::m_defaultDateTimeFormat / m_defaultDateFormat /
// m_defaultTimeFormat) backing the no-argument Date.prototype.toLocale*String
// paths are initialized once and never invalidated, so after a host TZ change
// they keep formatting in the old time zone while every other construction
// shape moves.
//
// Remove the //@ skip (and add the "skip if $hostOS == playstation" guard the
// sibling tests use) once the default formatters are invalidated on TZ change.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

if (!$vm.setHostTimeZone("America/Los_Angeles"))
    throw new Error("Failed to set host time zone to America/Los_Angeles");

setTimeout(() => {
    // 2024-06-15 00:00 UTC.
    //   In LA  (PDT, UTC-7): 2024-06-14 17:00 PDT.
    //   In JST (UTC+9):      2024-06-15 09:00 JST.
    const ms = Date.UTC(2024, 5, 15, 0, 0);
    const warm = new Date(ms);

    // Initialize all three per-global default formatters under LA time. Each
    // no-argument call must agree with its explicit-empty-options shape, which
    // builds a fresh formatter with the same required/defaults.
    const laAll = warm.toLocaleString();
    const laDate = warm.toLocaleDateString();
    const laTime = warm.toLocaleTimeString();
    expect("LA toLocaleString agrees with slow path", laAll, warm.toLocaleString(undefined, {}));
    expect("LA toLocaleDateString agrees with slow path", laDate, warm.toLocaleDateString(undefined, {}));
    expect("LA toLocaleTimeString agrees with slow path", laTime, warm.toLocaleTimeString(undefined, {}));

    if (!$vm.setHostTimeZone("Asia/Tokyo"))
        throw new Error("Failed to set host time zone to Asia/Tokyo");

    setTimeout(() => {
        expect("post-change tz",
            new Intl.DateTimeFormat().resolvedOptions().timeZone, "Asia/Tokyo");

        // A fresh Date cannot carry stale DateInstanceData; any disagreement
        // below belongs to the default formatters.
        const fresh = new Date(ms);
        expect("fresh JST hours", fresh.getHours(), 9);

        expect("JST toLocaleString agrees with slow path",
            fresh.toLocaleString(), fresh.toLocaleString(undefined, {}));
        expect("JST toLocaleDateString agrees with slow path",
            fresh.toLocaleDateString(), fresh.toLocaleDateString(undefined, {}));
        expect("JST toLocaleTimeString agrees with slow path",
            fresh.toLocaleTimeString(), fresh.toLocaleTimeString(undefined, {}));

        // LA and JST renderings differ in both calendar day and hour, so the
        // no-argument outputs must actually move.
        if (fresh.toLocaleString() === laAll)
            throw new Error(`toLocaleString did not move with the TZ change: "${laAll}"`);
        if (fresh.toLocaleDateString() === laDate)
            throw new Error(`toLocaleDateString did not move with the TZ change: "${laDate}"`);
        if (fresh.toLocaleTimeString() === laTime)
            throw new Error(`toLocaleTimeString did not move with the TZ change: "${laTime}"`);
    }, 0);
}, 0);
