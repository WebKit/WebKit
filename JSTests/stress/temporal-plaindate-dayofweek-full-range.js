//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected} (${message})`);
}

// Temporal.PlainDate.dayOfWeek is 1 (Monday) ... 7 (Sunday).
function dayOfWeekFromLegacy(isoString) {
    let day = new Date(isoString + "T00:00:00Z").getUTCDay();
    return day === 0 ? 7 : day;
}

const cases = [
    ["1970-01-01", 4],
    ["1969-12-31", 3],
    ["1970-01-04", 7],
    ["1970-01-05", 1],
    ["2000-01-01", 6],
    ["1600-01-01", 6],
    ["0001-01-01", 1],
    ["0000-12-31", 7],
    ["-000001-12-31", 5],
    ["+275760-09-13", 6],
    ["-271821-04-20", 2],
];

for (let [iso, expected] of cases) {
    let plainDate = Temporal.PlainDate.from(iso);
    shouldBe(plainDate.dayOfWeek, expected, iso);
    shouldBe(plainDate.dayOfWeek, dayOfWeekFromLegacy(iso), `${iso} vs Date`);
}

for (let year = -271820; year <= 275759; year += 3001) {
    for (let month = 1; month <= 12; month += 5) {
        let plainDate = new Temporal.PlainDate(year, month, 1);
        let iso = plainDate.toString();
        shouldBe(plainDate.dayOfWeek, dayOfWeekFromLegacy(iso), iso);
    }
}
