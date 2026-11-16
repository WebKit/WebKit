//@ requireOptions("--useTemporal=1")
//@ skip if $hostOS == "playstation"

// Companion to intl-datetimeformat-cache-dst-transition.js for Temporal.
// JSC only exposes a subset of Temporal to JS: there is no
// Temporal.ZonedDateTime, and Temporal.TimeZone exposes only toString/toJSON
// (no getOffsetNanosecondsFor), so nothing here can observe a time-zone offset
// directly. We therefore cover the two cache paths Temporal actually reaches:
//
//   1. Temporal.Now.timeZoneId() -> DateCache::defaultTimeZone()
//      -> timeZoneCache, which must invalidate after timeZoneDidChange().
//
//   2. Temporal.PlainDate.prototype.add -> TemporalCalendar::isoDateAdd
//      -> balanceISODate -> DateCache::yearMonthDayFromDaysWithCache, the same
//      year/month/day cache Date methods use, reached from a separate code
//      path. (PlainDate.from(isostring) parses directly and does not hit it.)

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

if (!$vm.setHostTimeZone("America/Los_Angeles"))
    throw new Error("Failed to set host time zone to America/Los_Angeles");

setTimeout(() => {
    // --- Temporal.Now.timeZoneId tracks the system TZ ---
    expect("Temporal.Now.timeZoneId", Temporal.Now.timeZoneId(), "America/Los_Angeles");

    // --- PlainDate arithmetic across the DST boundary ---
    // PlainDate is a wall-calendar object with no time zone, so DST cannot
    // change its arithmetic; what we are testing is that the shared
    // yearMonthDayFromDaysWithCache returns spec-correct values from the
    // Temporal code path. Walk a day at a time across spring-forward.
    const dayBefore = Temporal.PlainDate.from("2024-03-09");
    const dayOf     = dayBefore.add({ days: 1 });
    const dayAfter  = dayOf.add({ days: 1 });
    expect("PlainDate dayBefore", dayBefore.toString(), "2024-03-09");
    expect("PlainDate dayOf",     dayOf.toString(),     "2024-03-10");
    expect("PlainDate dayAfter",  dayAfter.toString(),  "2024-03-11");
    expect("PlainDate dayOf year",  dayOf.year,  2024);
    expect("PlainDate dayOf month", dayOf.month, 3);
    expect("PlainDate dayOf day",   dayOf.day,   10);

    // Larger jumps fall outside the adjacent-day fast path (it only fires when
    // the resulting day-of-month stays within 1..28), forcing the full
    // day->year/month/day conversion.
    const winter = Temporal.PlainDate.from("2024-01-15");
    const summer = winter.add({ months: 5 });    // -> 2024-06-15
    const nextWinter = summer.add({ months: 7 }); // -> 2025-01-15
    expect("PlainDate winter -> summer", summer.toString(), "2024-06-15");
    expect("PlainDate summer -> next winter", nextWinter.toString(), "2025-01-15");

    // Crossing the fall-back date in single-day steps.
    const fbBefore = Temporal.PlainDate.from("2024-11-02");
    const fbOf     = fbBefore.add({ days: 1 });
    const fbAfter  = fbOf.add({ days: 1 });
    expect("PlainDate fbBefore", fbBefore.toString(), "2024-11-02");
    expect("PlainDate fbOf",     fbOf.toString(),     "2024-11-03");
    expect("PlainDate fbAfter",  fbAfter.toString(),  "2024-11-04");
    expect("PlainDate fbOf dayOfWeek", fbOf.dayOfWeek, 7); // Sunday in ISO

    // --- Cross-API consistency ---
    // PlainDate and Date should agree on the calendar date of an instant in LA.
    // Date.UTC(2024, 10, 3, 10, 30) = 02:30 PST -> 2024-11-03 in LA.
    const d = new Date(Date.UTC(2024, 10, 3, 10, 30));
    expect("Date getDate matches PlainDate", d.getDate(), fbOf.day);
    expect("Date getMonth+1 matches PlainDate", d.getMonth() + 1, fbOf.month);
    expect("Date getFullYear matches PlainDate", d.getFullYear(), fbOf.year);

    // Flip TZ to verify Temporal.Now picks up the change.
    if (!$vm.setHostTimeZone("Asia/Tokyo"))
        throw new Error("Failed to set host time zone to Asia/Tokyo");

    setTimeout(() => {
        expect("Temporal.Now.timeZoneId after change",
            Temporal.Now.timeZoneId(), "Asia/Tokyo");

        // PlainDate arithmetic is TZ-independent, so it stays consistent.
        const stillDayOf = Temporal.PlainDate.from("2024-03-09").add({ days: 1 });
        expect("PlainDate post-change", stillDayOf.toString(), "2024-03-10");
    }, 0);
}, 0);
