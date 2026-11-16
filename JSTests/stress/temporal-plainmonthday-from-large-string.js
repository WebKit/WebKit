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

shouldThrow(() => Temporal.PlainMonthDay.from("a".repeat(1 << 20)), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from("x".repeat(1 << 16), { overflow: "constrain" }), RangeError);

shouldThrow(() => Temporal.PlainMonthDay.from("not-a-month-day"), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from(""), RangeError);
