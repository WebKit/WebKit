//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}

function shouldThrowRangeError(fn, msg) {
    let thrown = null;
    try { fn(); } catch (e) { thrown = e; }
    if (!thrown)
        throw new Error(`${msg}: expected RangeError but nothing was thrown`);
    if (!(thrown instanceof RangeError))
        throw new Error(`${msg}: expected RangeError but got ${thrown}`);
}

// ISO8601::parseDate() used to read past the end of the buffer for short
// truncated strings: month/day length guards only applied to the Date format.
// All of these must throw RangeError instead of crashing.
{
    const crashers = [
        ["PlainDate", "0"],
        ["PlainDate", "1"],
        ["PlainDate", "12"],
        ["PlainDate", "123"],
        ["PlainDate", "12-"],
        ["Instant", "1"],
        ["PlainTime", "1"],
        ["PlainDateTime", "12"],
        ["PlainMonthDay", "1"],
        ["PlainMonthDay", "12"],
        ["PlainMonthDay", "12-"],
        ["PlainMonthDay", "121"],
        ["PlainMonthDay", "--1"],
        ["PlainMonthDay", "--12"],
        ["PlainMonthDay", "--12-"],
        ["PlainMonthDay", "--121"],
        ["PlainMonthDay", "02-2"],
        ["PlainMonthDay", "2024-01"],
        ["PlainMonthDay", "202401"],
        ["PlainMonthDay", "2021-12"],
        ["PlainMonthDay", "2024-05"],
        ["PlainYearMonth", "2024-0"],
        ["PlainYearMonth", "2024-01-"],
        ["PlainYearMonth", "2024-01-1"],
        ["PlainYearMonth", "202401-"],
        ["PlainYearMonth", "2024011"],
        ["PlainYearMonth", "12-1"],
        ["ZonedDateTime", "1"],
        ["ZonedDateTime", "12"],
        ["PlainDate", "2024-0"],
        ["PlainDate", "2024-01-"],
        ["PlainDate", "2024-01-1"],
        ["PlainDate", "1976111"],
        ["PlainDate", "19761"],
    ];
    for (const [type, string] of crashers)
        shouldThrowRangeError(() => Temporal[type].from(string), `${type}.from(${JSON.stringify(string)})`);
}

// Valid strings must keep working.
{
    shouldBe(Temporal.PlainDate.from("2024-01-15").toString(), "2024-01-15", "PlainDate hyphenated");
    shouldBe(Temporal.PlainDate.from("20240115").toString(), "2024-01-15", "PlainDate compact");
    shouldBe(Temporal.PlainDate.from("+002024-01-15").toString(), "2024-01-15", "PlainDate extended year");
    shouldBe(Temporal.PlainDate.from("2024-01-15T12:30").toString(), "2024-01-15", "PlainDate with time");
    shouldBe(Temporal.PlainDate.from("2024-01-01[u-ca=iso8601]").toString(), "2024-01-01", "calendar annotation");

    shouldBe(Temporal.PlainYearMonth.from("2024-01").toString(), "2024-01", "YearMonth hyphenated");
    shouldBe(Temporal.PlainYearMonth.from("202401").toString(), "2024-01", "YearMonth compact");
    shouldBe(Temporal.PlainYearMonth.from("2024-01[u-ca=iso8601]").toString(), "2024-01", "YearMonth annotated");
    shouldBe(Temporal.PlainYearMonth.from("19761118").toString(), "1976-11", "YearMonth compact full date");
    shouldBe(Temporal.PlainYearMonth.from("1976-11-18").toString(), "1976-11", "YearMonth full date");
    shouldBe(Temporal.PlainYearMonth.from("1976-11-18T15:23:30").toString(), "1976-11", "YearMonth full datetime");
    shouldBe(Temporal.PlainYearMonth.from("19761118T15:23").toString(), "1976-11", "YearMonth compact datetime");

    shouldBe(Temporal.PlainMonthDay.from("12-14").toString(), "12-14", "MonthDay hyphenated");
    shouldBe(Temporal.PlainMonthDay.from("1214").toString(), "12-14", "MonthDay compact");
    shouldBe(Temporal.PlainMonthDay.from("--12-14").toString(), "12-14", "MonthDay double hyphen");
    shouldBe(Temporal.PlainMonthDay.from("--1214").toString(), "12-14", "MonthDay double hyphen compact");
    shouldBe(Temporal.PlainMonthDay.from("1130[u-ca=iso8601]").toString(), "11-30", "MonthDay annotated");
    shouldBe(Temporal.PlainMonthDay.from("1118[+01:00]").toString(), "11-18", "MonthDay timezone annotation");
    shouldBe(Temporal.PlainMonthDay.from("1976-11-18").toString(), "11-18", "MonthDay full date");
    shouldBe(Temporal.PlainMonthDay.from("1976-11-18T15:23:30").toString(), "11-18", "MonthDay full datetime");

    shouldBe(Temporal.PlainDateTime.from("2024-01-15T12:30").toString(), "2024-01-15T12:30:00", "PlainDateTime");
    shouldBe(Temporal.ZonedDateTime.from("2024-01-15T12:30[UTC]").toString(), "2024-01-15T12:30:00+00:00[UTC]", "ZonedDateTime");
}
