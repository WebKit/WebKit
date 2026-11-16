//@ skip if $hostOS == "playstation"

// Companion to intl-datetimeformat-stale-data-across-tz-change.js. That one holds a Date whose
// broken-down local time is the only one the VM has cached; this one first pushes that breakdown
// out of the cross-instance memo with other time values, so the held Date is reachable only from
// the instance itself.

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

    // Populate heldDate's cached breakdown with LA-relative values.
    expect("LA hours",  heldDate.getHours(), 17);
    expect("LA offset", heldDate.getTimezoneOffset(), 420);
    expect("LA date",   heldDate.getDate(), 14);
    expect("LA day",    heldDate.getDay(), 5); // Friday

    // Flood the cross-instance memo with distinct ms so heldDate's entry is evicted from it and
    // the only surviving copy of the LA breakdown is the one inside heldDate.
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
