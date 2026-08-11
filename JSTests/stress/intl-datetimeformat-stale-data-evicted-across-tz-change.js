//@ skip
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=318362
// Skipped: exposes a known, deferred bug (see the FIXME in
// DateInstanceCache::reset). Companion to
// intl-datetimeformat-stale-data-across-tz-change.js, which covers the case
// where the DateInstance's DateInstanceData is still resident in a cache slot
// when the time zone changes. This one covers the eviction case: the
// DateInstanceData is evicted from its slot (its slot overwritten by another
// ms) but stays alive via DateInstance::m_data, so DateCache::reset() never
// reaches it and the held Date keeps its stale local time.
//
// Remove the //@ skip (and add the "skip if $hostOS == playstation" guard the
// sibling tests use) once the eviction case is fixed.

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
    const heldMs = Date.UTC(2024, 5, 15, 0, 0);
    const heldDate = new Date(heldMs);

    // Populate heldDate.m_data with LA-relative cached values.
    expect("LA hours",  heldDate.getHours(), 17);
    expect("LA offset", heldDate.getTimezoneOffset(), 420);
    expect("LA date",   heldDate.getDate(), 14);
    expect("LA day",    heldDate.getDay(), 5); // Friday

    // Flood the 16-slot direct-mapped DateInstanceCache with distinct ms so
    // heldDate's slot is overwritten. Its DateInstanceData is now evicted from
    // the cache but still referenced by heldDate.m_data.
    for (let i = 1; i <= 256; i++)
        new Date(heldMs + i * 86400000).getHours();

    if (!$vm.setHostTimeZone("Asia/Tokyo"))
        throw new Error("Failed to set host time zone to Asia/Tokyo");

    setTimeout(() => {
        expect("post-change tz",
            new Intl.DateTimeFormat().resolvedOptions().timeZone, "Asia/Tokyo");

        // A fresh Date with the same ms correctly reflects Tokyo.
        const freshDate = new Date(heldMs);
        expect("fresh JST hours",  freshDate.getHours(), 9);
        expect("fresh JST offset", freshDate.getTimezoneOffset(), -540);

        // The held Date must agree (spec-correct), even though its evicted
        // m_data was populated under LA TZ.
        expect("held JST hours",  heldDate.getHours(), 9);
        expect("held JST offset", heldDate.getTimezoneOffset(), -540);
        expect("held JST date",   heldDate.getDate(), 15);
        expect("held JST day",    heldDate.getDay(), 6); // Saturday
    }, 0);
}, 0);
