//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}

const cases = [
    // [calendar, expected after +24h]
    ["iso8601"],
    ["hebrew"],
    ["gregory"],
    ["japanese"],
    ["islamic-umalqura"],
    ["chinese"],
    ["coptic"],
    ["ethiopic"],
    ["persian"],
];

for (const [cal] of cases) {
    const base = new Temporal.PlainDate(2024, 1, 15, cal);
    const plus24h = base.add({ hours: 24 });
    const plus1d = base.add({ days: 1 });
    shouldBe(plus24h.toString(), plus1d.toString(), `${cal}: add({hours: 24}) ≡ add({days: 1})`);
}

// Time-rollover should also work for subtract (negate-then-rollover).
for (const [cal] of cases) {
    const base = new Temporal.PlainDate(2024, 1, 15, cal);
    const minus24h = base.subtract({ hours: 24 });
    const minus1d = base.subtract({ days: 1 });
    shouldBe(minus24h.toString(), minus1d.toString(), `${cal}: subtract({hours: 24}) ≡ subtract({days: 1})`);
}

// 48h must roll to 2 days, not be ignored.
for (const [cal] of cases) {
    const base = new Temporal.PlainDate(2024, 1, 15, cal);
    const plus48h = base.add({ hours: 48 });
    const plus2d = base.add({ days: 2 });
    shouldBe(plus48h.toString(), plus2d.toString(), `${cal}: add({hours: 48}) ≡ add({days: 2})`);
}

// Mixed: 1 day + 24 hours = 2 days.
for (const [cal] of cases) {
    const base = new Temporal.PlainDate(2024, 1, 15, cal);
    const mixed = base.add({ days: 1, hours: 24 });
    const total = base.add({ days: 2 });
    shouldBe(mixed.toString(), total.toString(), `${cal}: add({days: 1, hours: 24}) ≡ add({days: 2})`);
}

// Sub-day amounts that don't accumulate to a full day stay on the same date.
for (const [cal] of cases) {
    const base = new Temporal.PlainDate(2024, 1, 15, cal);
    const subDay = base.add({ hours: 23, minutes: 59, seconds: 59 });
    shouldBe(subDay.toString(), base.toString(), `${cal}: sub-day duration stays on same date`);
}
