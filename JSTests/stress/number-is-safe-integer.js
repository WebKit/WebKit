// Test for Number.isSafeInteger()

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Actual: ${actual}, Expected: ${expected}`);
}

shouldBe(Number.hasOwnProperty("isSafeInteger"), true);
shouldBe(typeof Number.isSafeInteger, "function");

// Function properties.
shouldBe(Number.isSafeInteger.length, 1);
shouldBe(Number.isSafeInteger.name, "isSafeInteger");

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(Object.getOwnPropertyDescriptor(Number, "isSafeInteger").configurable, true);
    shouldBe(Object.getOwnPropertyDescriptor(Number, "isSafeInteger").enumerable, false);
    shouldBe(Object.getOwnPropertyDescriptor(Number, "isSafeInteger").writable, true);

    // Special
    shouldBe(Number.isSafeInteger(), false);
    shouldBe(Number.isSafeInteger(undefined), false);
    shouldBe(Number.isSafeInteger(null), false);

    // Numeric
    shouldBe(Number.isSafeInteger(0), true);
    shouldBe(Number.isSafeInteger(-0), true);
    shouldBe(Number.isSafeInteger(1), true);
    shouldBe(Number.isSafeInteger(-1), true);
    shouldBe(Number.isSafeInteger(1.0), true);
    shouldBe(Number.isSafeInteger(-1.0), true);
    shouldBe(Number.isSafeInteger(1.0), true);
    shouldBe(Number.isSafeInteger(-1.0), true);
    shouldBe(Number.isSafeInteger(42), true);
    shouldBe(Number.isSafeInteger(123.5), false);
    shouldBe(Number.isSafeInteger(-123.5), false);
    shouldBe(Number.isSafeInteger(1e10), true);
    shouldBe(Number.isSafeInteger(-1e10), true);
    shouldBe(Number.isSafeInteger(1.7e10), true);
    shouldBe(Number.isSafeInteger(-1.7e10), true);
    shouldBe(Number.isSafeInteger(1.7e-10), false);
    shouldBe(Number.isSafeInteger(-1.7e-10), false);

    shouldBe(Number.isSafeInteger(Number.MAX_VALUE), false);
    shouldBe(Number.isSafeInteger(Number.MIN_VALUE), false);
    shouldBe(Number.isSafeInteger(Number.MAX_SAFE_INTEGER), true);
    shouldBe(Number.isSafeInteger(Number.MAX_SAFE_INTEGER + 1), false);
    shouldBe(Number.isSafeInteger(Number.MIN_SAFE_INTEGER), true);
    shouldBe(Number.isSafeInteger(Number.MIN_SAFE_INTEGER - 1), false);
    shouldBe(Number.isSafeInteger(Math.PI), false);
    shouldBe(Number.isSafeInteger(Math.E), false);
    shouldBe(Number.isSafeInteger(Math.LOG2E), false);
    shouldBe(Number.isSafeInteger(Math.LOG10E), false);
    shouldBe(Number.isSafeInteger(Infinity), false);
    shouldBe(Number.isSafeInteger(-Infinity), false);
    shouldBe(Number.isSafeInteger(NaN), false);
    shouldBe(Number.isSafeInteger(1, 3), true);
    shouldBe(Number.isSafeInteger(1, 3.75), true);
    shouldBe(Number.isSafeInteger(Infinity, 3), false);

    // Non-numeric.
    shouldBe(Number.isSafeInteger({}), false);
    shouldBe(Number.isSafeInteger({ webkit: "cute" }), false);
    shouldBe(Number.isSafeInteger([]), false);
    shouldBe(Number.isSafeInteger([123]), false);
    shouldBe(Number.isSafeInteger([1, 1]), false);
    shouldBe(Number.isSafeInteger([NaN]), false);
    shouldBe(Number.isSafeInteger(""), false);
    shouldBe(Number.isSafeInteger("1"), false);
    shouldBe(Number.isSafeInteger("x"), false);
    shouldBe(Number.isSafeInteger("NaN"), false);
    shouldBe(Number.isSafeInteger("Infinity"), false);
    shouldBe(Number.isSafeInteger(true), false);
    shouldBe(Number.isSafeInteger(false), false);
    shouldBe(
        Number.isSafeInteger(function () {}),
        false
    );
    shouldBe(
        Number.isSafeInteger(() => {}),
        false
    );
    shouldBe(Number.isSafeInteger(Number.isSafeInteger), false);
    shouldBe(Number.isSafeInteger(Number.isSafeInteger()), false);
    shouldBe(Number.isSafeInteger(Symbol()), false);
    shouldBe(Number.isSafeInteger(new Number(123)), false);
    shouldBe(Number.isSafeInteger(new Number(-123)), false);
    shouldBe(Number.isSafeInteger(new Number("123")), false);
    shouldBe(Number.isSafeInteger(new Number(undefined)), false);
    shouldBe(Number.isSafeInteger(new Number(null)), false);
    shouldBe(Number.isSafeInteger(new Number(true)), false);
    shouldBe(Number.isSafeInteger(new Number(false)), false);

    // BigInt
    shouldBe(Number.isSafeInteger(BigInt(123)), false);
    shouldBe(Number.isSafeInteger(BigInt(-123)), false);
    shouldBe(Number.isSafeInteger(BigInt("123")), false);
    shouldBe(Number.isSafeInteger(BigInt("-123")), false);
    shouldBe(Number.isSafeInteger(BigInt("0x1fffffffffffff")), false);
    shouldBe(Number.isSafeInteger(BigInt("0o377777777777777777")), false);

    // Type conversion shouldn not happen.
    var objectWithNumberValueOf = {
        valueOf: function () {
            return 123;
        },
    };
    var objectWithNaNValueOf = {
        valueOf: function () {
            return NaN;
        },
    };
    shouldBe(Number.isSafeInteger(objectWithNumberValueOf), false);
    shouldBe(Number.isSafeInteger(objectWithNaNValueOf), false);

    var objectRecordConversionCalls = {
        callList: [],
        toString: function () {
            this.callList.push("toString");
            return "Bad";
        },
        valueOf: function () {
            this.callList.push("valueOf");
            return 12345;
        },
    };

    shouldBe(Number.isSafeInteger(objectRecordConversionCalls), false);
    shouldBe(objectRecordConversionCalls.callList.length, 0);
}
