//@ runDefault("--useDollarVM=1")

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`expected ${expected} but got ${actual}`);
}

function makeSubstring(string, suffix) {
    return (" " + string + suffix).slice(1, -suffix.length);
}

function test(string, expected) {
    shouldBe(Date.parse(string), expected);
    shouldBe(Date.parse(string), expected);
    shouldBe(new Date(string).getTime(), expected);

    Date.parse("1970-01-01");
    shouldBe(Date.parse($vm.make16BitStringIfPossible(string)), expected);

    for (let suffix of ["0", "9", "Z", ":30", " PM", "+09:00"]) {
        Date.parse("1970-01-01");
        shouldBe(Date.parse(makeSubstring(string, suffix)), expected);
    }
}

for (let i = 0; i < 20; ++i) {
    test("2026-11-05", 1793836800000);
    test("2026-11-05T12:34:56Z", 1793882096000);
    test("2026-11-05T12:34:56.789Z", 1793882096789);
    test("2026-11-05T12:34:56+09:00", 1793849696000);
    test("2026-11-05T12:34:56.7+09:00", 1793849696700);
    test("Thu, 05 Nov 2026 12:34:56 GMT", 1793882096000);
    test("Thu Nov 05 2026 12:34:56 GMT+0900 (Japan Standard Time)", 1793849696000);
    test("11/5/2026 12:34 PM UTC", 1793882040000);
    test("Nov 5, 2026 1:00 AM GMT+9", 1793808000000);

    test("", NaN);
    test(" ", NaN);
    test("(", NaN);
    test("Nov", NaN);
    test("2026-", NaN);
    test("2026-11-", NaN);
    test("2026-11-05T", NaN);
    test("2026-11-05T12:", NaN);
    test("2026-11-05T12:34:56.", NaN);
    test("2026-11-05T12:34:56+", NaN);
    test("2026-11-05\0", NaN);
    test("2026-11-05T12:34:56Z\0", NaN);

    test("2026-11-05T12:34:56Z\u00e9", NaN);
    test("Thu Nov 05 2026 12:34:56 GMT+0900 (\u00e9t\u00e9)", 1793849696000);
    test("11/5/2026,\u202f12:34:56\u202fPM UTC", 1793882096000);
    test("Thu Nov 05 2026 12:34:56 GMT+0900 (\u65e5\u672c\u6a19\u6e96\u6642)", 1793849696000);
}
