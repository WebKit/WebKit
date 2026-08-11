//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function shouldThrow(errorType, callback, message) {
    try {
        callback();
    } catch (error) {
        if (error instanceof errorType)
            return;
        throw new Error(`${message}: expected ${errorType.name}, got ${error}`);
    }
    throw new Error(`${message}: expected ${errorType.name}`);
}

// Hebrew is lunisolar but not Chinese/Dangi, so day clamping consults the ICU actual maximum for the month.
for (const [monthCode, ordinal, daysInMonth, iso] of [
    ["M02", 2, 29, "2023-11-13"],
    ["M04", 4, 29, "2024-01-10"],
    ["M05L", 6, 30, "2024-03-10"],
    ["M06", 7, 29, "2024-04-08"],
    ["M12", 13, 29, "2024-10-02"],
]) {
    const byCode = Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, monthCode, day: 31 });
    shouldBe(byCode.day, daysInMonth, `${monthCode} constrained day`);
    shouldBe(byCode.daysInMonth, daysInMonth, `${monthCode} daysInMonth`);
    shouldBe(byCode.monthCode, monthCode, `${monthCode} monthCode`);
    shouldBe(byCode.toString(), `${iso}[u-ca=hebrew]`, `${monthCode} constrained date`);

    const byOrdinal = Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, month: ordinal, day: 31 });
    shouldBe(byOrdinal.day, daysInMonth, `month ${ordinal} constrained day`);
    shouldBe(byOrdinal.monthCode, monthCode, `month ${ordinal} monthCode`);
    shouldBe(byOrdinal.toString(), `${iso}[u-ca=hebrew]`, `month ${ordinal} constrained date`);

    shouldThrow(RangeError, () => Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, monthCode, day: daysInMonth + 1 }, { overflow: "reject" }),
        `${monthCode} rejects out-of-range day`);
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, month: ordinal, day: daysInMonth + 1 }, { overflow: "reject" }),
        `month ${ordinal} rejects out-of-range day`);
}
