// Regression benchmark for the chinese/dangi monthCode path in nonISOCalendarDateToISO
// (CalendarICUBridge.cpp). ICU4C cannot say which ordinal a month code occupies, so the bridge
// anchors on the start of the reference year and walks month by month; each advance jumps to
// day 29 and then steps, since a lunar month is 29 or 30 days. The walk is memoized per
// (calendar, year, monthCode), and the month length per month start.
//
// One phase per thing to protect:
//   steady - a single input, so both memos hit every time
//   warm   - four distinct keys over both calendars, still inside the walk memo
//   cold   - every month code for both calendars, more keys than either memo holds
//
// The resolved day is checked once per case up front rather than in the loops: the day getter
// is a second ICU round trip that touches neither memo, so accumulating it would dilute what
// the loops measure.

const steadyCase = { calendar: "chinese", monthCode: "M07", day: 15 };

const warmCases = [
    { calendar: "chinese", monthCode: "M03", day: 15 },
    { calendar: "chinese", monthCode: "M05L", day: 15 },
    { calendar: "dangi", monthCode: "M07", day: 15 },
    { calendar: "dangi", monthCode: "M11", day: 15 },
];

const coldCases = [];
for (const calendar of ["chinese", "dangi"]) {
    for (let month = 1; month <= 12; ++month) {
        const monthCode = "M" + (month < 10 ? "0" : "") + month;
        coldCases.push({ calendar, monthCode, day: 15 });
        coldCases.push({ calendar, monthCode: monthCode + "L", day: 15 });
    }
}

if (coldCases.length !== 48)
    throw "Bad case count: " + coldCases.length;

// Day 15 is inside every lunar month, and under the default constrain overflow a leap month
// absent from the reference year falls back to the non-leap month, so every case resolves to
// day 15 rather than throwing.
for (const fields of [steadyCase, ...warmCases, ...coldCases]) {
    const day = Temporal.PlainMonthDay.from(fields).day;
    if (day !== 15)
        throw "Bad day for " + fields.calendar + " " + fields.monthCode + ": " + day;
}

let sink = null;
for (var i = 0; i < 10000; ++i)
    sink = Temporal.PlainMonthDay.from(steadyCase);
for (var i = 0; i < 2000; ++i) {
    for (var j = 0; j < warmCases.length; ++j)
        sink = Temporal.PlainMonthDay.from(warmCases[j]);
}
for (var i = 0; i < 5; ++i) {
    for (var j = 0; j < coldCases.length; ++j)
        sink = Temporal.PlainMonthDay.from(coldCases[j]);
}

if (!sink)
    throw "Bad result: " + sink;
