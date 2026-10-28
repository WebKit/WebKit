//@ skip if not $jitTests
//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`expected ${expected} but got ${actual}`);
}

let functionCount = 0;

function test(body, expectedForUndefined, expectedForNull, expectedForBigInt) {
    for (const [source, expected] of [["undefined", expectedForUndefined], ["null", expectedForNull]]) {
        const constant = new Function(`let value = ${source}; ${body} // ${functionCount++}`);
        noInline(constant);
        const argument = new Function("value", `${body} // ${functionCount++}`);
        noInline(argument);
        const value = source === "null" ? null : undefined;

        for (let i = 0; i < testLoopCount * 10; ++i) {
            shouldBe(constant(), expected);
            shouldBe(argument(value), expected);
        }
        shouldBe(numberOfDFGCompiles(constant) <= 1, true);
        shouldBe(numberOfDFGCompiles(argument) <= 1, true);

        for (let i = 0; i < testLoopCount; ++i) {
            shouldBe(argument(10n), expectedForBigInt);
            shouldBe(argument(value), expected);
        }
    }
}
noDFG(test);

test("return ++value;", NaN, 1, 11n);
test("return --value;", NaN, -1, 9n);
test("return value++;", NaN, 0, 10n);
test("return value--;", NaN, 0, 10n);
test("return ~value;", -1, -1, -11n);
