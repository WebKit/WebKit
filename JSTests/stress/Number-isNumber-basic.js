function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// Test the basic behaviors of Number.isFinite()
//
shouldBe(Number.hasOwnProperty("isFinite"), true);
shouldBe(typeof Number.isFinite, 'function');
shouldBe(Number.isFinite !== isFinite, true);

// Function properties.
shouldBe(Number.isFinite.length, 1);
shouldBe(Number.isFinite.name, 'isFinite');
shouldBe(Object.getOwnPropertyDescriptor(Number, "isFinite").configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Number, "isFinite").enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Number, "isFinite").writable, true);

// Some simple cases.
shouldBe(Number.isFinite(), false);
shouldBe(Number.isFinite(NaN), false);
shouldBe(Number.isFinite(undefined), false);

shouldBe(Number.isFinite(0), true);
shouldBe(Number.isFinite(-0), true);
shouldBe(Number.isFinite(1), true);
shouldBe(Number.isFinite(-1), true);
shouldBe(Number.isFinite(42), true);
shouldBe(Number.isFinite(123.5), true);
shouldBe(Number.isFinite(-123.5), true);
shouldBe(Number.isFinite(1e10), true);
shouldBe(Number.isFinite(-1e10), true);
shouldBe(Number.isFinite(1.7e10), true);
shouldBe(Number.isFinite(-1.7e10), true);

shouldBe(Number.isFinite(Number.MAX_VALUE), true);
shouldBe(Number.isFinite(Number.MIN_VALUE), true);
shouldBe(Number.isFinite(Number.MAX_SAFE_INTEGER), true);
shouldBe(Number.isFinite(Number.MIN_SAFE_INTEGER), true);
shouldBe(Number.isFinite(Math.PI), true);
shouldBe(Number.isFinite(Math.E), true);
shouldBe(Number.isFinite(Math.LOG2E), true);
shouldBe(Number.isFinite(Math.LOG10E), true);
shouldBe(Number.isFinite(Infinity), false);
shouldBe(Number.isFinite(-Infinity), false);
shouldBe(Number.isFinite(null), false);
shouldBe(Number.isFinite(1, 3), true);
shouldBe(Number.isFinite(Infinity, 3), false);

// Non-numeric.
shouldBe(Number.isFinite({}), false);
shouldBe(Number.isFinite({ webkit: "awesome" }), false);
shouldBe(Number.isFinite([]), false);
shouldBe(Number.isFinite([123]), false);
shouldBe(Number.isFinite([1,1]), false);
shouldBe(Number.isFinite([NaN]), false);
shouldBe(Number.isFinite(""), false);
shouldBe(Number.isFinite("1"), false);
shouldBe(Number.isFinite("x"), false);
shouldBe(Number.isFinite("NaN"), false);
shouldBe(Number.isFinite("Infinity"), false);
shouldBe(Number.isFinite(true), false);
shouldBe(Number.isFinite(false), false);
shouldBe(Number.isFinite(function(){}), false);
shouldBe(Number.isFinite(() => {}), false);
shouldBe(Number.isFinite(isFinite), false);
shouldBe(Number.isFinite(Symbol()), false);
shouldBe(Number.isFinite(new Number(123)), false);
shouldBe(Number.isFinite(new Number(-123)), false);
shouldBe(Number.isFinite(new Number("123")), false);
shouldBe(Number.isFinite(new Number(undefined)), false);
shouldBe(Number.isFinite(new Number(true)), false);
shouldBe(Number.isFinite(new Number(false)), false);

// BigInt
shouldBe(Number.isFinite(BigInt(123)), false);
shouldBe(Number.isFinite(BigInt(-123)), false);
shouldBe(Number.isFinite(BigInt("123")), false);
shouldBe(Number.isFinite(BigInt("-123")), false);
shouldBe(Number.isFinite(BigInt("0x1fffffffffffff")), false);
shouldBe(Number.isFinite(BigInt("0o377777777777777777")), false);

// Type conversion, doesn't happen.
var objectWithNumberValueOf = { valueOf: function() { return 123; } };
var objectWithNaNValueOf = { valueOf: function() { return NaN; } };
shouldBe(Number.isFinite(objectWithNumberValueOf), false);
shouldBe(Number.isFinite(objectWithNaNValueOf), false);

var objectRecordConversionCalls = {
    callList: [],
    toString: function() {
        this.callList.push("toString");
        return "Bad";
    },
    valueOf: function() {
        this.callList.push("valueOf");
        return 12345;
    }
};

shouldBe(Number.isFinite(objectRecordConversionCalls), false);
shouldBe(objectRecordConversionCalls.callList.length, 0);
