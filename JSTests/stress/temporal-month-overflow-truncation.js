//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, errorType, desc) {
    let threw = false;
    try { fn(); } catch(e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(desc + ": expected " + errorType.name + " got " + e.constructor.name + ": " + e.message);
    }
    if (!threw) throw new Error(desc + ": expected " + errorType.name + " but did not throw");
}

function shouldBe(a, e) { if (a !== e) throw new Error("expected " + JSON.stringify(e) + " got " + JSON.stringify(a)); }

// month=257 should NOT silently wrap to month=1 via uint8_t truncation.

// ZonedDateTime.with() era branch: month=257 with overflow:reject must throw.
{
    const zdt = Temporal.ZonedDateTime.from("2023-05-01T00:00+09:00[Asia/Tokyo][u-ca=japanese]");
    shouldThrow(
        () => zdt.with({ era: "reiwa", eraYear: 5, month: 257 }, { overflow: "reject" }),
        RangeError, "ZDT.with month=257 reject"
    );
    // With constrain: should clamp to valid month range, not wrap to 1.
    const r = zdt.with({ era: "reiwa", eraYear: 5, month: 257 }, { overflow: "constrain" });
    if (r.month === 1) throw new Error("month=257 constrain wrapped to 1 — truncation bug");
}

// PlainDate.with() non-ISO: month=257 with overflow:reject must throw.
{
    const pd = Temporal.PlainDate.from({ era: "reiwa", eraYear: 5, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(
        () => pd.with({ month: 257 }, { overflow: "reject" }),
        RangeError, "PlainDate.with month=257 reject"
    );
}

// PlainDate.from() non-ISO: month=257 with overflow:reject must throw.
{
    shouldThrow(
        () => Temporal.PlainDate.from({ era: "reiwa", eraYear: 5, month: 257, day: 1, calendar: "japanese" }, { overflow: "reject" }),
        RangeError, "PlainDate.from month=257 reject"
    );
}

// ISO PlainDate: month=257 reject throws.
{
    shouldThrow(
        () => Temporal.PlainDate.from({ year: 2023, month: 257, day: 1 }, { overflow: "reject" }),
        RangeError, "PlainDate.from ISO month=257 reject"
    );
    // ISO constrain: clamps to 12.
    const r = Temporal.PlainDate.from({ year: 2023, month: 257, day: 1 }, { overflow: "constrain" });
    shouldBe(r.month, 12);
}
