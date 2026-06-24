//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// era+eraYear must be provided together; Showa 10 (1935) is unambiguously mid-Showa.
{
    const ym = Temporal.PlainYearMonth.from({ era: "showa", eraYear: 10, month: 6, calendar: "japanese" });
    shouldBe(ym.year, 1935);
    shouldBe(ym.era, "showa");
    shouldBe(ym.eraYear, 10);

    // Change era+eraYear together — Showa 50 = 1975.
    const r1 = ym.with({ era: "showa", eraYear: 50 });
    shouldBe(r1.year, 1975);
    shouldBe(r1.eraYear, 50);
    shouldBe(r1.month, 6);

    // Switch to a different era — Heisei 5 = 1993.
    const r2 = ym.with({ era: "heisei", eraYear: 5 });
    shouldBe(r2.year, 1993);
    shouldBe(r2.eraYear, 5);
    shouldBe(r2.month, 6);

    // Change month only — no era fields — base era/eraYear preserved through merge.
    const r3 = ym.with({ month: 9 });
    shouldBe(r3.year, 1935);
    shouldBe(r3.era, "showa");
    shouldBe(r3.eraYear, 10);
    shouldBe(r3.month, 9);
}

// iso8601 (no eras) still works — era/eraYear must not be read for this calendar.
{
    const ym = new Temporal.PlainYearMonth(2020, 3);
    const result = ym.with({ year: 2024 });
    shouldBe(result.year, 2024);
    shouldBe(result.month, 3);
}
