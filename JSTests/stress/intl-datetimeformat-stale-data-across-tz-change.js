//@ skip if $hostOS == "playstation"

// Regression coverage for stale-cached-GregorianDateTime on a DateInstance
// across a time zone change.
//
// DateInstance lazily fetches a DateInstanceData from m_dateInstanceCache on
// the first accessor call and stores it in m_data. Subsequent accessors (in
// both the C++ slow path and the DFG/FTL inline fast paths) only re-compute
// when the receiver's ms changes (`m_gregorianDateTimeCachedForMS != milli`).
// After a TZ change, DateCache::clearForTimeZoneChange poisons that sentinel on
// the DateInstanceData still resident in the cache slots, so a DateInstance
// pointing at one of them takes a forced miss and recomputes against the new
// local zone.

// Spec: ECMA-262 LocalTime(t) uses "the LocalTZA of the current host
// environment", so the same Date must reflect the new TZ after the change.
// SpiderMonkey 115 agrees on every assertion below.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

if (!$vm.setHostTimeZone("America/Los_Angeles"))
    throw new Error("Failed to set host time zone to America/Los_Angeles");

setTimeout(() => {
    expect("initial tz",
        new Intl.DateTimeFormat().resolvedOptions().timeZone,
        "America/Los_Angeles");

    // 2024-06-15 00:00 UTC.
    //   In LA  (PDT, UTC-7): 2024-06-14 17:00 PDT.
    //   In JST (UTC+9):      2024-06-15 09:00 JST.
    const heldDate = new Date(Date.UTC(2024, 5, 15, 0, 0));

    // Populate the DateInstance's m_data with LA-relative cached values.
    expect("LA hours",  heldDate.getHours(), 17);
    expect("LA offset", heldDate.getTimezoneOffset(), 420);
    expect("LA date",   heldDate.getDate(), 14);
    expect("LA day",    heldDate.getDay(), 5); // Friday

    // Flip TZ.
    if (!$vm.setHostTimeZone("Asia/Tokyo"))
        throw new Error("Failed to set host time zone to Asia/Tokyo");

    setTimeout(() => {
        expect("post-change tz",
            new Intl.DateTimeFormat().resolvedOptions().timeZone,
            "Asia/Tokyo");

        // A *fresh* Date with the same ms correctly reflects Tokyo.
        const freshDate = new Date(Date.UTC(2024, 5, 15, 0, 0));
        expect("fresh JST hours",  freshDate.getHours(), 9);
        expect("fresh JST offset", freshDate.getTimezoneOffset(), -540);
        expect("fresh JST date",   freshDate.getDate(), 15);
        expect("fresh JST day",    freshDate.getDay(), 6); // Saturday

        // The held Date must agree (spec-correct), even though its m_data was
        // populated under LA TZ.
        expect("held JST hours",  heldDate.getHours(), 9);
        expect("held JST offset", heldDate.getTimezoneOffset(), -540);
        expect("held JST date",   heldDate.getDate(), 15);
        expect("held JST day",    heldDate.getDay(), 6);
    }, 0);
}, 0);
