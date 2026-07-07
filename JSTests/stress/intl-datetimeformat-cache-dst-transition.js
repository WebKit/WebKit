//@ skip if $hostOS == "playstation"

// Verifies that every DateCache cache entry returns spec-correct values for
// dates straddling a DST transition in the same time zone. After bug 314414,
// these caches persist across VM entries on non-Cocoa platforms, so they must
// answer correctly on both sides of a DST boundary without being flushed.
//
// America/Los_Angeles 2024:
//   - Spring forward: 2024-03-10 02:00 PST -> 03:00 PDT (UTC-8 -> UTC-7)
//   - Fall back:      2024-11-03 02:00 PDT -> 01:00 PST (UTC-7 -> UTC-8)

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

function expectContains(label, haystack, needle)
{
    if (typeof haystack !== "string" || haystack.indexOf(needle) < 0)
        throw new Error(`${label}: expected ${JSON.stringify(haystack)} to contain ${JSON.stringify(needle)}`);
}

if (!$vm.setHostTimeZone("America/Los_Angeles"))
    throw new Error("Failed to set host time zone to America/Los_Angeles");

// Reach a new VMEntryScope so the time zone change takes effect.
setTimeout(() => {
    expect("tz applied",
        new Intl.DateTimeFormat().resolvedOptions().timeZone,
        "America/Los_Angeles");

    // UTC instants either side of the spring-forward transition.
    const pstUTC = new Date(Date.UTC(2024, 2, 10,  9, 30)); // -> 01:30 PST
    const pdtUTC = new Date(Date.UTC(2024, 2, 10, 11, 30)); // -> 04:30 PDT

    // --- DSTCache (m_caches[UTCTime]) ---
    // Either side of the transition should give the spec-correct local offset,
    // both on cache miss and on cache hit.
    expect("PST offset",       pstUTC.getTimezoneOffset(), 480);
    expect("PDT offset",       pdtUTC.getTimezoneOffset(), 420);
    expect("PST offset (hit)", pstUTC.getTimezoneOffset(), 480);
    expect("PDT offset (hit)", pdtUTC.getTimezoneOffset(), 420);

    // --- DateInstanceCache (m_dateInstanceCache) ---
    // Re-querying the same Date should be consistent.
    expect("PST hours",        pstUTC.getHours(), 1);
    expect("PST hours (hit)",  pstUTC.getHours(), 1);
    expect("PDT hours",        pdtUTC.getHours(), 4);
    expect("PDT hours (hit)",  pdtUTC.getHours(), 4);
    // A fresh Date object with the same ms hits the cache directly.
    expect("PDT hours (new instance hit)",
        new Date(pdtUTC.getTime()).getHours(), 4);

    // --- YearMonthDayCache (m_yearMonthDayCache) ---
    // Three distinct Dates, all 16:00 UTC, on consecutive local days bracketing
    // the DST transition. The cache stores (days, year, month, day); querying
    // adjacent days exercises the +/-1 fast path.
    const day9  = new Date(Date.UTC(2024, 2,  9, 16, 0)); // 2024-03-09 08:00 PST
    const day10 = new Date(Date.UTC(2024, 2, 10, 16, 0)); // 2024-03-10 09:00 PDT
    const day11 = new Date(Date.UTC(2024, 2, 11, 16, 0)); // 2024-03-11 09:00 PDT
    expect("day9  year",  day9.getFullYear(),  2024);
    expect("day10 year",  day10.getFullYear(), 2024);
    expect("day11 year",  day11.getFullYear(), 2024);
    expect("day9  month", day9.getMonth(),  2);
    expect("day10 month", day10.getMonth(),  2);
    expect("day11 month", day11.getMonth(),  2);
    expect("day9  date",  day9.getDate(),   9);
    expect("day10 date",  day10.getDate(), 10);
    expect("day11 date",  day11.getDate(), 11);
    expect("day9  hours PST", day9.getHours(),   8);
    expect("day10 hours PDT", day10.getHours(),  9);
    expect("day11 hours PDT", day11.getHours(),  9);

    // --- parseDate cache (m_cachedDateString / m_cachedDateStringValue) ---
    // ES2015+ : ISO date-time strings without a tz designator are local time.
    // The parser subtracts the local offset, so DST must be respected.
    expect("parse PST",       Date.parse("2024-03-10T01:30:00"),
        Date.UTC(2024, 2, 10,  9, 30));
    expect("parse PDT",       Date.parse("2024-03-10T04:30:00"),
        Date.UTC(2024, 2, 10, 11, 30));
    // Cache hit (same string twice in a row should bypass parsing).
    expect("parse PDT (hit)", Date.parse("2024-03-10T04:30:00"),
        Date.UTC(2024, 2, 10, 11, 30));
    expect("parse PST (hit)", Date.parse("2024-03-10T01:30:00"),
        Date.UTC(2024, 2, 10,  9, 30));

    // --- m_timeZoneStandardDisplayNameCache / m_timeZoneDSTDisplayNameCache ---
    // Date#toString embeds "(Pacific Standard Time)" or "(Pacific Daylight
    // Time)" depending on whether the instant is in DST. Both caches are
    // populated on the first call; subsequent calls hit the cache.
    const stdStr1 = pstUTC.toString();
    const dstStr1 = pdtUTC.toString();
    expectContains("PST display name", stdStr1, "(Pacific Standard Time)");
    expectContains("PDT display name", dstStr1, "(Pacific Daylight Time)");
    expectContains("PST GMT offset",   stdStr1, "GMT-0800");
    expectContains("PDT GMT offset",   dstStr1, "GMT-0700");
    // Cache hits.
    expectContains("PST display name (hit)",
        new Date(pstUTC.getTime()).toString(), "(Pacific Standard Time)");
    expectContains("PDT display name (hit)",
        new Date(pdtUTC.getTime()).toString(), "(Pacific Daylight Time)");

    // --- DSTCache (m_caches[LocalTime]) ---
    // Constructing a Date from local components uses the LocalTime cache. Pick
    // dates well away from the discontinuity so the local->UTC mapping is
    // unambiguous.
    const winterLocal = new Date(2024, 0, 15, 12, 0); // 2024-01-15 12:00 PST
    const summerLocal = new Date(2024, 5, 15, 12, 0); // 2024-06-15 12:00 PDT
    expect("local PST -> UTC ms",
        winterLocal.getTime(), Date.UTC(2024, 0, 15, 20, 0));
    expect("local PDT -> UTC ms",
        summerLocal.getTime(), Date.UTC(2024, 5, 15, 19, 0));
    expect("local PST offset",  winterLocal.getTimezoneOffset(), 480);
    expect("local PDT offset",  summerLocal.getTimezoneOffset(), 420);

    // --- Intl.DateTimeFormat (separate code path from Date#toString) ---
    // The IntlDateTimeFormat instance caches its resolved time zone; it relies
    // on ICU's per-instant DST awareness, not on DateCache's display-name caches.
    const dtfShort = new Intl.DateTimeFormat("en-US", {
        timeZone: "America/Los_Angeles", hour: "2-digit", minute: "2-digit",
        hour12: false, timeZoneName: "short",
    });
    const dtfLong = new Intl.DateTimeFormat("en-US", {
        timeZone: "America/Los_Angeles", hour: "2-digit", minute: "2-digit",
        hour12: false, timeZoneName: "long",
    });
    expectContains("DTF short PST", dtfShort.format(pstUTC), "PST");
    expectContains("DTF short PDT", dtfShort.format(pdtUTC), "PDT");
    expectContains("DTF long PST",  dtfLong.format(pstUTC),  "Pacific Standard Time");
    expectContains("DTF long PDT",  dtfLong.format(pdtUTC),  "Pacific Daylight Time");

    // formatToParts surfaces the timeZoneName as a discrete part.
    const findPart = (parts, type) => parts.find(p => p.type === type)?.value;
    const partsPST = dtfShort.formatToParts(pstUTC);
    const partsPDT = dtfShort.formatToParts(pdtUTC);
    expect("DTF parts PST", findPart(partsPST, "timeZoneName"), "PST");
    expect("DTF parts PDT", findPart(partsPDT, "timeZoneName"), "PDT");

    // --- Date#toLocaleString (Intl pathway reached via the Date API) ---
    const localePST = pstUTC.toLocaleString("en-US", { timeZoneName: "short" });
    const localePDT = pdtUTC.toLocaleString("en-US", { timeZoneName: "short" });
    expectContains("toLocaleString PST", localePST, "PST");
    expectContains("toLocaleString PDT", localePDT, "PDT");

    // --- setters: read-local-modify-write-back-to-UTC roundtrip ---
    // Date#setHours decodes the receiver's ms to local components (UTCTime
    // cache), mutates one field, then re-encodes (LocalTime cache). A single
    // call exercises both directions in one step.
    const pstSetter = new Date(Date.UTC(2024, 2, 10,  9, 30)); // 01:30 PST
    pstSetter.setHours(0); // 00:30 PST -> UTC 08:30
    expect("setHours PST roundtrip",
        pstSetter.getTime(), Date.UTC(2024, 2, 10, 8, 30));

    const pdtSetter = new Date(Date.UTC(2024, 2, 10, 11, 30)); // 04:30 PDT
    pdtSetter.setHours(6); // 06:30 PDT -> UTC 13:30
    expect("setHours PDT roundtrip",
        pdtSetter.getTime(), Date.UTC(2024, 2, 10, 13, 30));

    // setDate that walks the receiver across the spring-forward boundary: the
    // local time-of-day must be preserved (10:00 local) while UTC ms shifts by
    // 23h, not 24h, because the day in between is only 23 wall-clock hours.
    const walker = new Date(Date.UTC(2024, 2,  9, 18, 0)); // 2024-03-09 10:00 PST
    expect("walker pre hours",    walker.getHours(), 10);
    expect("walker pre offset",   walker.getTimezoneOffset(), 480);
    walker.setDate(10);                                   // 2024-03-10 10:00 PDT
    expect("walker post hours",   walker.getHours(), 10);
    expect("walker post offset",  walker.getTimezoneOffset(), 420);
    expect("walker post UTC ms",  walker.getTime(), Date.UTC(2024, 2, 10, 17, 0));

    // setMonth across DST: jump from a PST instant to a PDT month, preserving
    // local time-of-day.
    const jumper = new Date(Date.UTC(2024, 0, 15, 20, 0)); // 2024-01-15 12:00 PST
    jumper.setMonth(5);                                    // 2024-06-15 12:00 PDT
    expect("setMonth hours preserved", jumper.getHours(), 12);
    expect("setMonth offset PDT",      jumper.getTimezoneOffset(), 420);
    expect("setMonth UTC ms",          jumper.getTime(), Date.UTC(2024, 5, 15, 19, 0));

    // --- getDay (day-of-week sentinel) ---
    // Across DST, day-of-week must reflect the local day. Pick instants whose
    // UTC day differs from their local day to make the assertion meaningful.
    //   2024-03-10 06:00 UTC = 2024-03-09 22:00 PST -> Saturday (6)
    //   2024-03-10 08:00 UTC = 2024-03-10 00:00 PST -> Sunday   (0)
    //   2024-03-10 11:00 UTC = 2024-03-10 04:00 PDT -> Sunday   (0)
    expect("getDay Sat PST",
        new Date(Date.UTC(2024, 2, 10,  6, 0)).getDay(), 6);
    expect("getDay Sun PST",
        new Date(Date.UTC(2024, 2, 10,  8, 0)).getDay(), 0);
    expect("getDay Sun PDT",
        new Date(Date.UTC(2024, 2, 10, 11, 0)).getDay(), 0);

    // --- Fall back: DST -> standard transition in the same direction ---
    // 2024-11-03 08:30 UTC = 01:30 PDT (still summer time)
    // 2024-11-03 10:30 UTC = 02:30 PST (after fall back)
    const fbPDT = new Date(Date.UTC(2024, 10, 3,  8, 30));
    const fbPST = new Date(Date.UTC(2024, 10, 3, 10, 30));
    expect("fallBack PDT offset", fbPDT.getTimezoneOffset(), 420);
    expect("fallBack PST offset", fbPST.getTimezoneOffset(), 480);
    expect("fallBack PDT hours",  fbPDT.getHours(), 1);
    expect("fallBack PST hours",  fbPST.getHours(), 2);
    expectContains("fallBack PDT display name",
        fbPDT.toString(), "(Pacific Daylight Time)");
    expectContains("fallBack PST display name",
        fbPST.toString(), "(Pacific Standard Time)");
}, 0);
