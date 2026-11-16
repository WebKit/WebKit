//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

// CalendarICUBridge.cpp memoizes two lunisolar results in small direct-mapped tables shared by
// chinese and dangi: the month-code walk (16 slots) and the month length (4 slots). One entry per
// slot, so every store can evict an unrelated key, and a broken tag check answers a lookup with a
// neighbour's value. Asserting a single expected field cannot see that, so everything below
// compares a value against the same value read at another time, or against an independent oracle.

// A CalendarID is the index of the identifier in intlAvailableCalendars(), which is the same
// sorted table Intl.supportedValuesOf("calendar") exposes.
const availableCalendars = Intl.supportedValuesOf("calendar");
const calendarId = { chinese: availableCalendars.indexOf("chinese"), dangi: availableCalendars.indexOf("dangi") };
shouldBe(calendarId.chinese >= 0 && calendarId.dangi >= 0, true, "chinese and dangi are available");

// Mirrors nonISOCalendarDateToISO:
//   packedMonthCode = (monthNumber << 1) | isLeapMonth
//   walkSlot = ((year * 31) ^ packedMonthCode ^ (calendarId * 7)) & (lunisolarWalkMemoCount - 1)
// lunisolarWalkMemoCount is 16. No slot number is written down below: the colliding pairs are
// searched for at run time, so this keeps testing real collisions if the mixing or size changes.
const walkMemoCount = 16;
function walkSlot({ calendar, year, monthCode }) {
    const monthNumber = Number(monthCode.substring(1, 3));
    const packedMonthCode = (monthNumber << 1) | (monthCode.length > 3 ? 1 : 0);
    return ((year * 31) ^ packedMonthCode ^ (calendarId[calendar] * 7)) & (walkMemoCount - 1);
}

const searchSpace = [];
for (const calendar of ["chinese", "dangi"]) {
    for (let year = 2019; year <= 2026; ++year) {
        for (let monthNumber = 1; monthNumber <= 12; ++monthNumber) {
            const monthCode = `M${monthNumber < 10 ? "0" : ""}${monthNumber}`;
            searchSpace.push({ calendar, year, monthCode });
            // A leap code missing from its year constrains to a neighbouring month; that answer is
            // memoized too, and it is the only way to vary the packed leap bit.
            searchSpace.push({ calendar, year, monthCode: `${monthCode}L` });
        }
    }
}

const bySlot = new Map();
for (const entry of searchSpace) {
    const slot = walkSlot(entry);
    if (!bySlot.has(slot))
        bySlot.set(slot, []);
    bySlot.get(slot).push(entry);
}

// Same calendar plus same year plus same slot implies the same packed month code, so a pair must
// differ in the calendar or the year; both kinds are picked, plus one that flips the leap bit.
// Already-chosen pairs are excluded, since one pair can satisfy several of the predicates and
// would otherwise be selected repeatedly instead of widening what is covered.
const chosen = new Set();
function findCollidingPair(name, predicate) {
    for (const [slot, entries] of bySlot) {
        for (let i = 0; i < entries.length; ++i) {
            for (let j = i + 1; j < entries.length; ++j) {
                const pairKey = `${keyOf(entries[i])} | ${keyOf(entries[j])}`;
                if (chosen.has(pairKey) || !predicate(entries[i], entries[j]))
                    continue;
                chosen.add(pairKey);
                return { name, slot, entries: [entries[i], entries[j]] };
            }
        }
    }
    throw new Error(`no unused same-slot collision found for ${name}`);
}

const collisions = [
    findCollidingPair("different calendar", (a, b) => a.calendar !== b.calendar),
    findCollidingPair("different year", (a, b) => a.calendar === b.calendar && a.year !== b.year),
    findCollidingPair("different leap bit", (a, b) => (a.monthCode.length > 3) !== (b.monthCode.length > 3)),
];

function keyOf({ calendar, year, monthCode }) {
    return `${calendar} ${year} ${monthCode}`;
}

// Everything the walk memo decides: the ISO date it lands on, the ordinal month it counted, and
// the month it settled on. An aliased hit changes at least one of them.
function walkSignature({ calendar, year, monthCode }) {
    const date = Temporal.PlainDate.from({ calendar, year, monthCode, day: 1 });
    return `${date.toString()} m=${date.month} mc=${date.monthCode} d=${date.day} dim=${date.daysInMonth}`;
}

// Record each answer exactly once, before any of the re-reads below.
const recorded = new Map();
for (const collision of collisions) {
    for (const entry of collision.entries) {
        if (!recorded.has(keyOf(entry)))
            recorded.set(keyOf(entry), walkSignature(entry));
    }
}

// If a colliding pair agreed, swapping the two answers would be invisible and the re-reads would
// pass for the wrong reason.
for (const { name, slot, entries } of collisions) {
    shouldBe(recorded.get(keyOf(entries[0])) !== recorded.get(keyOf(entries[1])), true,
        `slot ${slot} pair (${name}) is distinguishable: ${keyOf(entries[0])} vs ${keyOf(entries[1])}`);
}

// Re-read interleaved, so each lookup is preceded by a store into its own slot from another key.
for (const { name, slot, entries } of collisions) {
    for (let round = 0; round < 4; ++round) {
        for (const entry of entries)
            shouldBe(walkSignature(entry), recorded.get(keyOf(entry)), `slot ${slot} (${name}) aliasing for ${keyOf(entry)}`);
    }
}

// Month-length memo: 4 slots keyed by the month the cursor sits in, so consecutive days hit one
// slot repeatedly while the other calendar and the neighbouring months evict it.
const walkCalendars = ["chinese", "dangi"];
const walkLength = 400;
const observations = new Map(walkCalendars.map(calendar => [calendar, []]));
let isoDate = Temporal.PlainDate.from("2023-11-05");
for (let i = 0; i < walkLength; ++i) {
    // Alternate which calendar reads first so the eviction order varies.
    for (const calendar of (i % 2) ? walkCalendars.slice().reverse() : walkCalendars) {
        const date = isoDate.withCalendar(calendar);
        observations.get(calendar).push({ iso: isoDate.toString(), monthCode: date.monthCode, day: date.day, daysInMonth: date.daysInMonth });
    }
    isoDate = isoDate.add({ days: 1 });
}

for (const calendar of walkCalendars) {
    const days = observations.get(calendar);
    for (let i = days.length - 1; i >= 0; --i) {
        const expected = days[i];
        const date = Temporal.PlainDate.from(expected.iso).withCalendar(calendar);
        shouldBe(date.monthCode, expected.monthCode, `${calendar} ${expected.iso} monthCode re-read in reverse`);
        shouldBe(date.day, expected.day, `${calendar} ${expected.iso} day re-read in reverse`);
        shouldBe(date.daysInMonth, expected.daysInMonth, `${calendar} ${expected.iso} daysInMonth re-read in reverse`);
    }

    // Independent of any memo: consecutive days carrying one month code must agree on the length,
    // number their days consecutively, and -- when the whole month is inside the walk -- span
    // exactly that many days starting at day 1.
    let runStart = 0;
    for (let i = 1; i <= days.length; ++i) {
        if (i < days.length && days[i].monthCode === days[runStart].monthCode)
            continue;
        const daysInMonth = days[runStart].daysInMonth;
        shouldBe(daysInMonth === 29 || daysInMonth === 30, true, `${calendar} ${days[runStart].monthCode} length is 29 or 30`);
        for (let j = runStart; j < i; ++j) {
            shouldBe(days[j].daysInMonth, daysInMonth, `${calendar} ${days[j].iso} daysInMonth inside ${days[runStart].monthCode}`);
            shouldBe(days[j].day, days[runStart].day + (j - runStart), `${calendar} ${days[j].iso} day inside ${days[runStart].monthCode}`);
        }
        if (runStart > 0 && i < days.length) {
            shouldBe(days[runStart].day, 1, `${calendar} ${days[runStart].monthCode} run starts on day 1`);
            shouldBe(i - runStart, daysInMonth, `${calendar} ${days[runStart].monthCode} run length matches daysInMonth`);
        }
        runStart = i;
    }
}

// Oracle reached through the ordinal-month path instead of the walk above: a year's months must
// tile it exactly, each be a real lunation, and carry distinct codes.
for (const calendar of walkCalendars) {
    for (const year of [2022, 2023, 2024, 2026]) {
        const yearStart = Temporal.PlainDate.from({ calendar, year, month: 1, day: 1 });
        const monthsInYear = yearStart.monthsInYear;
        shouldBe(monthsInYear === 12 || monthsInYear === 13, true, `${calendar} ${year} monthsInYear is 12 or 13`);
        const monthCodes = new Set();
        let total = 0;
        for (let month = 1; month <= monthsInYear; ++month) {
            const date = Temporal.PlainDate.from({ calendar, year, month, day: 1 });
            shouldBe(date.month, month, `${calendar} ${year} ordinal month ${month} round-trips`);
            shouldBe(date.day, 1, `${calendar} ${year} ordinal month ${month} starts on day 1`);
            const daysInMonth = date.daysInMonth;
            shouldBe(daysInMonth === 29 || daysInMonth === 30, true, `${calendar} ${year} ordinal month ${month} length is 29 or 30`);
            monthCodes.add(date.monthCode);
            total += daysInMonth;
        }
        shouldBe(monthCodes.size, monthsInYear, `${calendar} ${year} month codes are distinct`);
        shouldBe(total, yearStart.daysInYear, `${calendar} ${year} months sum to daysInYear`);
    }
}
