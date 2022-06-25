function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertThrowTypeError(input) {
    try {
        eval(input);
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
}

assert("a" + 100n, "a100");
assert(128n + "baba", "128baba");

assertThrowTypeError("10n + 30");
assertThrowTypeError("36 + 15n");
assertThrowTypeError("120n + 30.5");
assertThrowTypeError("44.5 + 112034n");

assertThrowTypeError("10n - 30");
assertThrowTypeError("36 - 15n");
assertThrowTypeError("120n - 30.5");
assertThrowTypeError("44.5 - 112034n");

assertThrowTypeError("10n * 30");
assertThrowTypeError("36 * 15n");
assertThrowTypeError("120n * 30.5");
assertThrowTypeError("44.5 * 112034n");

assertThrowTypeError("10n / 30");
assertThrowTypeError("36 / 15n");
assertThrowTypeError("120n / 30.5");
assertThrowTypeError("44.5 / 112034n");

assertThrowTypeError("10n ** 30");
assertThrowTypeError("36 ** 15n");
assertThrowTypeError("120n ** 30.5");
assertThrowTypeError("44.5 ** 112034n");

