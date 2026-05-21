//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(fn, type) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
}

// Bug: canBeYear() returned true for "1118[+01:00]" because it only checked
// 4 leading digits and length >= 6, without verifying that the character
// after the digits is '-' or a digit (not '['). This caused the parser to
// try to interpret "1118" as year 1118, fail to find a month/day, and reject
// the whole string instead of treating it as MMDD with a timezone annotation.

// Compact MMDD format with timezone annotation — must be accepted
shouldBe(Temporal.PlainMonthDay.from("1118[+01:00]").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("1118[-05:00]").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("0101[UTC]").toString(), "01-01");
shouldBe(Temporal.PlainMonthDay.from("1231[America/New_York]").toString(), "12-31");

// Already-working formats remain correct
shouldBe(Temporal.PlainMonthDay.from("1118").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("11-18").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("--11-18").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("--1118").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("11-18[+01:00]").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("--11-18[+01:00]").toString(), "11-18");
shouldBe(Temporal.PlainMonthDay.from("1972-11-18[+01:00]").toString(), "11-18");

// Full year with timezone annotation still parses correctly as full date
shouldBe(Temporal.PlainMonthDay.from("1972-11-18[+01:00]").monthCode, "M11");
shouldBe(Temporal.PlainMonthDay.from("2020-06-15[America/Los_Angeles]").toString(), "06-15");

// canBeYear boundary: 4-digit string followed by digit/hyphen is still a year
shouldBe(Temporal.PlainMonthDay.from("1972-11-18").toString(), "11-18");

// Invalid strings still throw
shouldThrow(() => Temporal.PlainMonthDay.from("9999[+01:00]"), RangeError); // month 99 invalid
shouldThrow(() => Temporal.PlainMonthDay.from("0000[+01:00]"), RangeError); // month 00 invalid
