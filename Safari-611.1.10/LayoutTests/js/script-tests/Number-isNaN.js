description("Test the basic behaviors of Number.isNaN()");

shouldBeTrue('Number.hasOwnProperty("isNaN")');
shouldBeEqualToString('typeof Number.isNaN', 'function');
shouldBeTrue('Number.isNaN !== isNaN');

// Function properties.
shouldBe('Number.isNaN.length', '1');
shouldBeEqualToString('Number.isNaN.name', 'isNaN');
shouldBe('Object.getOwnPropertyDescriptor(Number, "isNaN").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(Number, "isNaN").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(Number, "isNaN").writable', 'true');

// Some simple cases.
shouldBeFalse('Number.isNaN()');
shouldBeTrue('Number.isNaN(NaN)');
shouldBeFalse('Number.isNaN(undefined)');

shouldBeFalse('Number.isNaN(0)');
shouldBeFalse('Number.isNaN(-0)');
shouldBeFalse('Number.isNaN(1)');
shouldBeFalse('Number.isNaN(-1)');
shouldBeFalse('Number.isNaN(42)');
shouldBeFalse('Number.isNaN(123.5)');
shouldBeFalse('Number.isNaN(-123.5)');

shouldBeFalse('Number.isNaN(Number.MAX_VALUE)');
shouldBeFalse('Number.isNaN(Number.MIN_VALUE)');
shouldBeFalse('Number.isNaN(Number.MAX_SAFE_INTEGER)');
shouldBeFalse('Number.isNaN(Number.MIN_SAFE_INTEGER)');
shouldBeFalse('Number.isNaN(Math.PI)');
shouldBeFalse('Number.isNaN(Math.E)');
shouldBeFalse('Number.isNaN(Infinity)');
shouldBeFalse('Number.isNaN(-Infinity)');
shouldBeFalse('Number.isNaN(null)');

// Non-numeric.
shouldBeFalse('Number.isNaN({})');
shouldBeFalse('Number.isNaN({ webkit: "awesome" })');
shouldBeFalse('Number.isNaN([])');
shouldBeFalse('Number.isNaN([123])');
shouldBeFalse('Number.isNaN([1,1])');
shouldBeFalse('Number.isNaN([NaN])');
shouldBeFalse('Number.isNaN("")');
shouldBeFalse('Number.isNaN("1")');
shouldBeFalse('Number.isNaN("x")');
shouldBeFalse('Number.isNaN("NaN")');
shouldBeFalse('Number.isNaN("Infinity")');
shouldBeFalse('Number.isNaN(true)');
shouldBeFalse('Number.isNaN(false)');
shouldBeFalse('Number.isNaN(function(){})');
shouldBeFalse('Number.isNaN(isNaN)');
shouldBeFalse('Number.isNaN(Symbol())');

// Type conversion, doesn't happen.
var objectWithNumberValueOf = { valueOf: function() { return 123; } };
var objectWithNaNValueOf = { valueOf: function() { return NaN; } };
shouldBeFalse('Number.isNaN(objectWithNumberValueOf)');
shouldBeFalse('Number.isNaN(objectWithNaNValueOf)');

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
shouldBeFalse('Number.isNaN(objectRecordConversionCalls)');
shouldBe('objectRecordConversionCalls.callList.length', '0');
