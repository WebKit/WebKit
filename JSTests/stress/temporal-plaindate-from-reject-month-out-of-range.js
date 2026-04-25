//@ requireOptions("--useTemporal=1")

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but got ${error}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const rejectOpt = { overflow: "reject" };
const hugeMonths = [13, 255, 256, 1e300, Number.MAX_SAFE_INTEGER, Number.MAX_VALUE, 2 ** 31, 2 ** 32, 2 ** 53];

for (const m of hugeMonths) {
    shouldThrow(() => Temporal.PlainDate.from({ year: 2000, month: m, day: 1 }, rejectOpt), RangeError);
    shouldThrow(() => Temporal.PlainDateTime.from({ year: 2000, month: m, day: 1 }, rejectOpt), RangeError);
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2000, month: m }, rejectOpt), RangeError);
    shouldThrow(() => Temporal.PlainMonthDay.from({ month: m, day: 1 }, rejectOpt), RangeError);
}

{
    const d = Temporal.PlainDate.from({ year: 2000, month: 1e300, day: 1 }, { overflow: "constrain" });
    shouldBe(d.month, 12);
    shouldBe(d.day, 1);
}
{
    const d = Temporal.PlainDate.from({ year: 2000, month: 12, day: 1e300 }, { overflow: "constrain" });
    shouldBe(d.month, 12);
    shouldBe(d.day, 31);
}
{
    const d = Temporal.PlainDateTime.from({ year: 2000, month: 1e300, day: 1e300 }, { overflow: "constrain" });
    shouldBe(d.month, 12);
    shouldBe(d.day, 31);
}
{
    const d = Temporal.PlainYearMonth.from({ year: 2000, month: 1e300 }, { overflow: "constrain" });
    shouldBe(d.month, 12);
}
{
    const d = Temporal.PlainMonthDay.from({ month: 1e300, day: 1 }, { overflow: "constrain" });
    shouldBe(d.monthCode, "M12");
}
