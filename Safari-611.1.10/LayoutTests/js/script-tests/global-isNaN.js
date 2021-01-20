description("Test the basic behaviors of global isNaN()");

var globalObject = (1,eval)("this");

shouldBeTrue('globalObject.hasOwnProperty("isNaN")');
shouldBeEqualToString('typeof isNaN', 'function');

// Function properties.
shouldBe('isNaN.length', '1');
shouldBeEqualToString('isNaN.name', 'isNaN');
shouldBe('Object.getOwnPropertyDescriptor(globalObject, "isNaN").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(globalObject, "isNaN").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(globalObject, "isNaN").writable', 'true');

// Some simple cases.
shouldBeTrue('isNaN()');
shouldBeTrue('isNaN(NaN)');
shouldBeTrue('isNaN(undefined)');

shouldBeFalse('isNaN(0)');
shouldBeFalse('isNaN(-0)');
shouldBeFalse('isNaN(1)');
shouldBeFalse('isNaN(-1)');
shouldBeFalse('isNaN(42)');
shouldBeFalse('isNaN(123.5)');
shouldBeFalse('isNaN(-123.5)');

shouldBeFalse('isNaN(Number.MAX_VALUE)');
shouldBeFalse('isNaN(Number.MIN_VALUE)');
shouldBeFalse('isNaN(Number.MAX_SAFE_INTEGER)');
shouldBeFalse('isNaN(Number.MIN_SAFE_INTEGER)');
shouldBeFalse('isNaN(Math.PI)');
shouldBeFalse('isNaN(Math.E)');
shouldBeFalse('isNaN(Infinity)');
shouldBeFalse('isNaN(-Infinity)');
shouldBeFalse('isNaN(null)');

// Non-numeric.
shouldBeTrue('isNaN({})');
shouldBeTrue('isNaN({ webkit: "awesome" })');
shouldBeFalse('isNaN([])');
shouldBeFalse('isNaN([123])');
shouldBeTrue('isNaN([1,1])');
shouldBeTrue('isNaN([NaN])');
shouldBeFalse('isNaN("")');
shouldBeFalse('isNaN("1")');
shouldBeTrue('isNaN("x")');
shouldBeTrue('isNaN("NaN")');
shouldBeFalse('isNaN("Infinity")');
shouldBeFalse('isNaN(true)');
shouldBeFalse('isNaN(false)');
shouldBeTrue('isNaN(function(){})');
shouldBeTrue('isNaN(isNaN)');
shouldThrow('isNaN(Symbol())');

// Type conversion.
var objectConvertToString = { toString: function() { return "42"; } };
shouldBeFalse('isNaN(objectConvertToString)');

var objectRecordToStringCall = { toStringCallCount: 0, toString: function() { this.toStringCallCount += 1; return "42"; } };
shouldBeFalse('isNaN(objectRecordToStringCall)');
shouldBe('objectRecordToStringCall.toStringCallCount', '1');

var objectThrowOnToString = { toString: function() { throw "No!"; } };
shouldThrow('isNaN(objectThrowOnToString)');

var objectWithValueOf = { valueOf: function() { return 1.1; } };
shouldBeFalse('isNaN(objectWithValueOf)');

var objectThrowOnValueOf = { valueOf: function() { throw "Nope!" }, toString: function() { return 5; } };
shouldThrow('isNaN(objectThrowOnValueOf)');

var objectRecordValueOfCall = { valueOfCallCount: 0, valueOf: function() { ++this.valueOfCallCount; return NaN; } }
shouldBeTrue('isNaN(objectRecordValueOfCall)');
shouldBe('objectRecordValueOfCall.valueOfCallCount', '1');

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
shouldBeFalse('isNaN(objectRecordConversionCalls)');
shouldBe('objectRecordConversionCalls.callList.toString()', '"valueOf"');
