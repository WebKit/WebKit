//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

// Past the threshold the getters report the ISO month and day verbatim. That is the invariant the
// arithmetic below has to agree with.
{
    const d = Temporal.PlainDate.from('+010001-06-15').withCalendar('chinese');
    shouldBe(d.year, 10001);
    shouldBe(d.monthCode, 'M06');
    shouldBe(d.day, 15);
}

// add() and until() must produce answers rather than failing.
for (const calendar of ['chinese', 'dangi']) {
    for (const year of [100000, 270000, -270000]) {
        const a = Temporal.PlainDate.from({ calendar, year, monthCode: 'M01', day: 1 });
        shouldBe(a.year, year);

        shouldBe(a.add({ years: 1 }).year, year + 1);
        shouldBe(a.add({ months: 1 }).monthCode, 'M02');
        shouldBe(a.add({ days: 1 }).day, 2);
        shouldBe(a.subtract({ years: 1 }).year, year - 1);

        const b = a.add({ years: 1 });
        shouldBe(a.until(b, { largestUnit: 'year' }).toString(), 'P1Y');
        shouldBe(a.since(b, { largestUnit: 'year' }).toString(), '-P1Y');
        shouldBe(b.until(a, { largestUnit: 'year' }).toString(), '-P1Y');

        // add and until have to stay mutually consistent out here.
        const shifted = a.add({ years: 3, months: 2, days: 5 });
        const diff = a.until(shifted, { largestUnit: 'year' });
        shouldBe(diff.toString(), 'P3Y2M5D');
        shouldBe(a.add(diff).equals(shifted), true);
    }
}

// Spans with only one endpoint past the threshold must also answer rather than fail.
{
    const near = Temporal.PlainDate.from({ calendar: 'chinese', year: 9000, monthCode: 'M01', day: 1 });
    const far = Temporal.PlainDate.from({ calendar: 'chinese', year: 270000, monthCode: 'M01', day: 1 });
    const forward = near.until(far, { largestUnit: 'year' });
    if (forward.years <= 0)
        throw new Error(`expected a positive year count, got ${forward.toString()}`);
    const backward = far.until(near, { largestUnit: 'year' });
    if (backward.years >= 0)
        throw new Error(`expected a negative year count, got ${backward.toString()}`);

    const min = Temporal.PlainDate.from({ calendar: 'chinese', year: -270000, monthCode: 'M01', day: 1 });
    const max = Temporal.PlainDate.from({ calendar: 'chinese', year: 270000, monthCode: 'M01', day: 1 });
    shouldBe(min.until(max, { largestUnit: 'year' }).toString(), 'P540000Y');
}

// Day and week diffs were already calendar-independent and must not change.
{
    const a = Temporal.PlainDate.from({ calendar: 'chinese', year: 270000, monthCode: 'M01', day: 1 });
    shouldBe(a.until(a.add({ days: 366 }), { largestUnit: 'day' }).toString(), 'P366D');
    shouldBe(a.until(a.add({ days: 366 }), { largestUnit: 'week' }).toString(), 'P52W2D');
}

// Inside the trustworthy range nothing changes: these stay on the astronomical path.
{
    const a = Temporal.PlainDate.from({ calendar: 'chinese', year: 2024, monthCode: 'M01', day: 1 });
    shouldBe(a.until(a.add({ years: 1 }), { largestUnit: 'year' }).toString(), 'P1Y');
    shouldBe(a.until(a.add({ years: 1 }), { largestUnit: 'month' }).toString(), 'P12M');
    const leap = Temporal.PlainDate.from({ calendar: 'chinese', year: 2004, monthCode: 'M02L', day: 1 });
    shouldBe(leap.until(leap.add({ years: 1 }), { largestUnit: 'month' }).toString(), 'P12M');
}
