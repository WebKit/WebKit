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

shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", 2 ** 32 + 1972),
    "2**32 + 1972 should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(2, 29, "iso8601", 2 ** 53),
    "2**53 should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", -(2 ** 53)),
    "-2**53 should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", 1e9),
    "1e9 should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", -1e9),
    "-1e9 should throw RangeError");

shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", 275761),
    "275761 should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(7, 15, "iso8601", -271822),
    "-271822 should throw RangeError");

assert(new Temporal.PlainMonthDay(7, 15, "iso8601", 275760).toString() === "07-15",
    "reference year 275760 (July is within limits) should be accepted");
assert(new Temporal.PlainMonthDay(7, 15, "iso8601", -271821).toString() === "07-15",
    "reference year -271821 (July is within limits) should be accepted");
assert(new Temporal.PlainMonthDay(7, 15, "iso8601", 2000).toString() === "07-15",
    "reference year 2000 should be accepted");
assert(new Temporal.PlainMonthDay(2, 29).toString() === "02-29",
    "default reference year 1972 should accept Feb 29");

shouldThrowRangeError(() => new Temporal.PlainMonthDay(2, 29, "iso8601", 1973),
    "Feb 29 in non-leap reference year should throw RangeError");
shouldThrowRangeError(() => new Temporal.PlainMonthDay(2, 30),
    "Feb 30 should throw RangeError");
