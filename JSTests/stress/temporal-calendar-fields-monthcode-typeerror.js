//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type, message) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
    if (message !== undefined && err.message !== message)
        throw new Error(`Expected message "${message}" but got "${err.message}"`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

const typeErrorMessage = "monthCode must be a string";

for (const badMonthCode of [42, true]) {
    shouldThrow(() => Temporal.PlainDate.from({ year: 2020, month: 1, day: 1, monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainDateTime.from({ year: 2020, month: 1, day: 1, monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2020, month: 1, monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainMonthDay.from({ day: 1, monthCode: badMonthCode }), TypeError, typeErrorMessage);

    shouldThrow(() => Temporal.PlainDate.from("2020-01-01").with({ monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainDateTime.from("2020-01-01T00:00").with({ monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainYearMonth.from("2020-01").with({ monthCode: badMonthCode }), TypeError, typeErrorMessage);
    shouldThrow(() => Temporal.PlainMonthDay.from({ day: 1, monthCode: "M01" }).with({ monthCode: badMonthCode }), TypeError, typeErrorMessage);
}

// Sanity: grammar-invalid (but string) monthCode still RangeErrors, not TypeErrors.
// "M99" is grammatically valid (any 2-digit M-code parses) but out of ISO's 1-12 range,
// so use "M00" (bare, no "L") — the grammar explicitly rejects it (only "M00L" is valid).
shouldThrow(() => Temporal.PlainDate.from({ year: 2020, day: 1, monthCode: "M00" }), RangeError, "Invalid monthCode");

// Sanity: a valid monthCode still works normally.
shouldBe(Temporal.PlainDate.from({ year: 2020, day: 1, monthCode: "M01" }).toString(), "2020-01-01");
shouldBe(Temporal.PlainDate.from("2020-01-01").with({ monthCode: "M02" }).toString(), "2020-02-01");
