//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

{
    const dates = [];
    for (let i = 0; i < 10000; i++) {
        dates.push(Temporal.PlainDate.from({ year: 2020 + (i % 50), month: (i % 12) + 1, day: (i % 28) + 1 }));
    }
    shouldBe(dates.length, 10000, "created 10k PlainDates");
    shouldBe(dates[9999].year, 2020 + (9999 % 50), "last date year correct");
}

{
    const durations = [];
    for (let i = 0; i < 10000; i++) {
        durations.push(new Temporal.Duration(0, 0, 0, i, i % 24, i % 60));
    }
    shouldBe(durations.length, 10000, "created 10k Durations");
    shouldBe(durations[5000].days, 5000, "mid duration days");
}

{
    const zdts = [];
    const baseNs = 1700000000000000000n;
    for (let i = 0; i < 1000; i++)
        zdts.push(new Temporal.ZonedDateTime(baseNs + BigInt(i) * 86400000000000n, "UTC"));
    shouldBe(zdts.length, 1000, "created 1k ZDTs");
}

{
    let date = Temporal.PlainDate.from("2020-01-01");
    for (let i = 0; i < 10000; i++) {
        const next = date.add({ days: 1 });
        if (i === 9999) {
            // 2020-01-01 + 10000 days = 2047-05-19
            shouldBe(next.year, 2047, "hot loop: final year");
        }
        date = next;
    }
}

{
    let total = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (let i = 0; i < 5000; i++) {
        total = Temporal.Duration.from({ hours: total.hours + 1, minutes: total.minutes + 30 });
    }
    shouldBe(total.hours, 5000, "accumulated hours");
    shouldBe(total.minutes, 150000, "accumulated minutes");
}

{
    let time = Temporal.PlainTime.from("00:00:00");
    for (let i = 0; i < 5000; i++) {
        time = time.add({ seconds: 17 });
    }
    // 5000 * 17 = 85000 seconds = 23h 36m 40s (mod 24h)
    shouldBe(time.hour, 23, "hot loop time hour");
    shouldBe(time.minute, 36, "hot loop time minute");
    shouldBe(time.second, 40, "hot loop time second");
}

{
    const instants = [];
    for (let i = 0; i < 1000; i++) {
        instants.push(Temporal.Instant.fromEpochNanoseconds(BigInt(i) * 1000000000n));
    }
    for (let i = 1; i < instants.length; i++) {
        const cmp = Temporal.Instant.compare(instants[i - 1], instants[i]);
        shouldBe(cmp, -1, `instant ${i} should be after ${i - 1}`);
    }
}

{
    let prev = Temporal.Now.instant();
    for (let i = 0; i < 100; i++) {
        const curr = Temporal.Now.instant();
        const cmp = Temporal.Instant.compare(prev, curr);
        if (cmp > 0) throw new Error(`Now.instant() not monotonic at iteration ${i}`);
        prev = curr;
    }
}
