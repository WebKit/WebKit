//@ requireOptions("--useTemporal=1")

function assert(cond, msg) {
    if (!cond) throw new Error("FAIL: " + msg);
}

function shouldThrowRangeError(fn, msg) {
    let thrown = null;
    try {
        fn();
    } catch (e) {
        thrown = e;
    }
    assert(thrown instanceof RangeError, msg + " (got: " + thrown + ")");
}

const overflows = [undefined, { overflow: "constrain" }, { overflow: "reject" }];

for (const options of overflows) {
    const tag = " (options: " + JSON.stringify(options) + ")";
    shouldThrowRangeError(() => Temporal.PlainDate.from({ calendar: "chinese", year: 2024, monthCode: "M00L", day: 1 }, options),
        "PlainDate chinese M00L should throw RangeError" + tag);
    shouldThrowRangeError(() => Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, monthCode: "M00L", day: 1 }, options),
        "PlainDate hebrew M00L should throw RangeError" + tag);
    shouldThrowRangeError(() => Temporal.PlainDateTime.from({ year: 2024, monthCode: "M00L", day: 1, calendar: "chinese" }, options),
        "PlainDateTime chinese M00L should throw RangeError" + tag);
    shouldThrowRangeError(() => Temporal.PlainDateTime.from({ year: 2016, monthCode: "M00L", day: 1, calendar: "japanese" }, options),
        "PlainDateTime japanese M00L should throw RangeError" + tag);
    shouldThrowRangeError(() => Temporal.PlainDate.from({ year: 2024, monthCode: "M00L", day: 1 }, options),
        "PlainDate iso8601 M00L should throw RangeError" + tag);
}
shouldThrowRangeError(() => Temporal.PlainMonthDay.from({ calendar: "chinese", monthCode: "M00L", day: 1 }),
    "PlainMonthDay chinese M00L should throw RangeError");
shouldThrowRangeError(() => Temporal.PlainYearMonth.from({ calendar: "chinese", year: 2024, monthCode: "M00L" }),
    "PlainYearMonth chinese M00L should throw RangeError");
shouldThrowRangeError(() => Temporal.PlainDate.from("2024-03-10[u-ca=chinese]").with({ monthCode: "M00L" }),
    "PlainDate.with chinese M00L should throw RangeError");

{
    const constrained = Temporal.PlainDate.from({ calendar: "chinese", year: 2024, monthCode: "M01L", day: 1 });
    assert(constrained.monthCode === "M01", "chinese M01L should constrain to M01, got " + constrained.monthCode);
    assert(constrained.year === 2024, "chinese M01L constrain should stay in year 2024, got " + constrained.year);
    const leapYear = Temporal.PlainDate.from({ calendar: "hebrew", year: 5779, monthCode: "M05L", day: 1 });
    assert(leapYear.monthCode === "M05L", "hebrew M05L in leap year 5779 should resolve to M05L, got " + leapYear.monthCode);
    const commonYear = Temporal.PlainDate.from({ calendar: "hebrew", year: 5781, monthCode: "M05L", day: 1 });
    assert(commonYear.monthCode === "M06", "hebrew M05L in common year 5781 should constrain to M06, got " + commonYear.monthCode);
}
