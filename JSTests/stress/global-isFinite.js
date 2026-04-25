function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Actual: ${actual}, Expected: ${expected}`);
}

function shouldBeTrue(actual) {
    if (!actual)
        throw new Error(`bad result: should be true`);
}

function shouldBeFalse(actual) {
    if (actual)
        throw new Error(`bad result: should be false`);
}

function shouldThrow(func) {
    var hasThrown = false;
    try {
        func()
    } catch {
        hasThrown = true;
    }
    if (!hasThrown)
        throw new Error(`bad result: should throw error`);
}


shouldBe(typeof isFinite, 'function');
shouldBeTrue(isFinite === isFinite);

// Function properties.
shouldBe(isFinite.length, 1);
shouldBe(isFinite.name, 'isFinite');

// Some simple cases.
shouldBeFalse(isFinite());
shouldBeFalse(isFinite(NaN));
shouldBeFalse(isFinite(undefined));

shouldBeTrue(isFinite(0));
shouldBeTrue(isFinite(-0));
shouldBeTrue(isFinite(1));
shouldBeTrue(isFinite(-1));
shouldBeTrue(isFinite(42));
shouldBeTrue(isFinite(123.5));
shouldBeTrue(isFinite(-123.5));
shouldBeTrue(isFinite(1e10));
shouldBeTrue(isFinite(-1e10));
shouldBeTrue(isFinite(1.7e10));
shouldBeTrue(isFinite(-1.7e10));

shouldBeTrue(isFinite(Number.MAX_VALUE));
shouldBeTrue(isFinite(Number.MIN_VALUE));
shouldBeTrue(isFinite(Number.MAX_SAFE_INTEGER));
shouldBeTrue(isFinite(Number.MIN_SAFE_INTEGER));
shouldBeTrue(isFinite(Math.PI));
shouldBeTrue(isFinite(Math.E));
shouldBeTrue(isFinite(Math.LOG2E));
shouldBeTrue(isFinite(Math.LOG10E));
shouldBeFalse(isFinite(Infinity));
shouldBeFalse(isFinite(-Infinity));
shouldBeTrue(isFinite(null));
shouldBeTrue(isFinite(1, 3));
shouldBeFalse(isFinite(Infinity, 3));

// Non-numeric.
shouldBeFalse(isFinite({}));
shouldBeFalse(isFinite({ webkit: "awesome" }));
shouldBeTrue(isFinite([]));
shouldBeTrue(isFinite([123]));
shouldBeFalse(isFinite([1,1]));
shouldBeFalse(isFinite([NaN]));
shouldBeTrue(isFinite(""));
shouldBeTrue(isFinite("1"));
shouldBeFalse(isFinite("x"));
shouldBeFalse(isFinite("NaN"));
shouldBeFalse(isFinite("Infinity"));
shouldBeTrue(isFinite(true));
shouldBeTrue(isFinite(false));
shouldBeFalse(isFinite(function(){}));
shouldBeFalse(isFinite(()=>{}));
shouldBeFalse(isFinite(isFinite));
shouldThrow(() => isFinite(Symbol()));
shouldBeTrue(isFinite(new Number(123)));
shouldBeTrue(isFinite(new Number(-123)));
shouldBeTrue(isFinite(new Number("123")));
shouldBeFalse(isFinite(new Number(undefined)));
shouldBeTrue(isFinite(new Number(null)));
shouldBeTrue(isFinite(new Number(true)));
shouldBeTrue(isFinite(new Number(false)));
shouldThrow(() => isFinite(BigInt(123)));
shouldThrow(() => isFinite(BigInt(-123)));
shouldThrow(() => isFinite(BigInt("123")));
shouldThrow(() => isFinite(BigInt("-123")));
shouldThrow(() => isFinite(BigInt("0x1fffffffffffff")));
shouldThrow(() => isFinite(BigInt("0o377777777777777777")));

// Type conversion, doesn't happen.
var objectWithNumberValueOf = { valueOf: function() { return 123; } };
var objectWithNaNValueOf = { valueOf: function() { return NaN; } };
shouldBeTrue(isFinite(objectWithNumberValueOf));
shouldBeFalse(isFinite(objectWithNaNValueOf));

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
shouldBeTrue(isFinite(objectRecordConversionCalls));
shouldBe(objectRecordConversionCalls.callList.length, 1);
shouldBe(objectRecordConversionCalls.callList.toString(), "valueOf");
