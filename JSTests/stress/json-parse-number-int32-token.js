function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected: " + expected);
}

var numbers = [
    "0", "-0", "1", "-1", "9", "10", "42", "123", "1023",
    "99999", "100000", "999999999", "-999999999",
    "1000000000", "-1000000000", "2147483647", "2147483648",
    "-2147483648", "-2147483649", "4294967295", "4294967296",
    "0.5", "-0.5", "1.5", "123.456", "0.0", "-0.0", "0.1", "-0.0001",
    "1e2", "1E2", "1e-2", "1e+2", "-1e2", "1e0", "0e0", "-0e0",
    "1e308", "1e309", "-1e309", "5e-324", "3.141592653589793",
    "123456789012345678901234567890", "9007199254740993",
    "900000000", "90000000", "9000000", "900000", "90000", "9000", "900", "90",
];

for (var i = 0; i < 1e3; ++i) {
    for (var s of numbers) {
        var expected = Number(s);
        shouldBe(JSON.parse(s), expected);
        shouldBe(JSON.parse(" " + s + " "), expected);
        var array = JSON.parse("[" + s + "," + s + "]");
        shouldBe(array[0], expected);
        shouldBe(array[1], expected);
        shouldBe(JSON.parse('{"a":' + s + "}").a, expected);
        shouldBe(JSON.parse('{"あ":' + s + "}")["あ"], expected);
        shouldBe(JSON.parse("[" + s + ",\"あ\"]")[0], expected);
        shouldBe(JSON.parse("[" + s + "]", function (key, value) { return value; })[0], expected);
        shouldBe(JSON.parse('{"a":{"b":[' + s + "]}}").a.b[0], expected);
    }
}

var invalid = ["01", "-", "+1", ".5", "1.", "1e", "1e+", "0x10", "Infinity", "NaN", "- 1", "--1", "1..2", "1e2e3"];
for (var s of invalid) {
    for (var source of [s, "[" + s + "]", '{"a":' + s + "}"]) {
        var threw = false;
        try {
            JSON.parse(source);
        } catch (error) {
            threw = true;
        }
        shouldBe(threw, true);
    }
}
