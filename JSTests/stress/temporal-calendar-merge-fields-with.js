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

// --- PlainDate.prototype.with ---

shouldBe(Temporal.PlainDate.from("2020-06-15").with({ day: 20 }).toString(), "2020-06-20");
shouldBe(Temporal.PlainDate.from("2020-06-15").with({ month: 3 }).toString(), "2020-03-15");
shouldBe(Temporal.PlainDate.from("2020-06-15").with({ year: 2021 }).toString(), "2021-06-15");

shouldBe(Temporal.PlainDate.from({ year: 2020, month: 5, day: 1, calendar: "hebrew" }).with({ day: 10 }).toString(), "-001741-12-28[u-ca=hebrew]");
shouldBe(Temporal.PlainDate.from({ year: 2020, month: 5, day: 1, calendar: "hebrew" }).with({ year: 2021 }).toString(), "-001739-01-07[u-ca=hebrew]");

// Chinese (lunisolar): year changes with month/monthCode absent from the partial — the
// case the removed lunisolarYearChange special case targeted (month must fall back via
// monthCode, not a raw ordinal, since leap-month insertion can shift the mapping).
shouldBe(Temporal.PlainDate.from({ year: 2020, month: 5, day: 1, calendar: "chinese" }).with({ year: 2023 }).toString(), "2023-05-19[u-ca=chinese]");
shouldBe(Temporal.PlainDate.from({ year: 2020, month: 5, day: 1, calendar: "chinese" }).with({ day: 10 }).toString(), "2020-06-01[u-ca=chinese]");

shouldBe(Temporal.PlainDate.from({ era: "reiwa", eraYear: 3, month: 5, day: 1, calendar: "japanese" }).with({ month: 8 }).toString(), "2021-08-01[u-ca=japanese]");

// --- PlainYearMonth.prototype.with ---

shouldBe(Temporal.PlainYearMonth.from("2020-06").with({ month: 3 }).toString(), "2020-03");
shouldBe(Temporal.PlainYearMonth.from({ year: 2020, month: 5, calendar: "hebrew" }).with({ month: 2 }).toString(), "-001741-09-21[u-ca=hebrew]");
shouldBe(Temporal.PlainYearMonth.from({ year: 2020, month: 5, calendar: "chinese" }).with({ year: 2023 }).toString(), "2023-05-19[u-ca=chinese]");

// --- PlainMonthDay.prototype.with ---

shouldBe(Temporal.PlainMonthDay.from({ month: 6, day: 15 }).with({ day: 20 }).toString(), "06-20");
shouldBe(Temporal.PlainMonthDay.from({ month: 6, day: 15 }).with({ month: 3 }).toString(), "03-15");
shouldBe(Temporal.PlainMonthDay.from({ monthCode: "M05", day: 1, calendar: "hebrew" }).with({ day: 10 }).toString(), "1972-01-26[u-ca=hebrew]");
shouldBe(Temporal.PlainMonthDay.from({ monthCode: "M05", day: 1, calendar: "hebrew" }).with({ month: 2, year: 2020 }).toString(), "1972-10-09[u-ca=hebrew]");

// Non-ISO month given without year (or era+eraYear): now deferred to
// nonISOResolveFields's own "year property must be present" check instead of a
// PlainMonthDay.prototype.with-specific message — same TypeError kind either way.
shouldThrow(() => Temporal.PlainMonthDay.from({ monthCode: "M05", day: 1, calendar: "hebrew" }).with({ month: 2 }), TypeError, "year property must be present");
