description("Test the basic behaviors of Math.clz32()");

shouldBeTrue('Math.hasOwnProperty("clz32")');
shouldBeEqualToString('typeof Math.clz32', 'function');
shouldBe('Object.getPrototypeOf(Math).clz32', 'undefined');

// Function properties.
shouldBe('Math.clz32.length', '1');
shouldBeEqualToString('Math.clz32.name', 'clz32');
shouldBe('Object.getOwnPropertyDescriptor(Math, "clz32").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(Math, "clz32").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(Math, "clz32").writable', 'true');

// Some simple cases.
shouldBe('Math.clz32(0)', '32');
shouldBe('Math.clz32(-0)', '32');
shouldBe('Math.clz32(1)', '31');
shouldBe('Math.clz32(-1)', '0');
shouldBe('Math.clz32(42)', '26');
shouldBe('Math.clz32(-2147483648)', '0');
shouldBe('Math.clz32(2147483647)', '1');

shouldBe('Math.clz32(Number.MAX_VALUE)', '32');
shouldBe('Math.clz32(Number.MIN_VALUE)', '32');
shouldBe('Math.clz32(Number.MAX_SAFE_INTEGER)', '0');
shouldBe('Math.clz32(Number.MIN_SAFE_INTEGER)', '31');

shouldBe('Math.clz32(Math.PI)', '30');
shouldBe('Math.clz32(Math.E)', '30');
shouldBe('Math.clz32(NaN)', '32');
shouldBe('Math.clz32(Number.POSITIVE_INFINITY)', '32');
shouldBe('Math.clz32(Number.NEGATIVE_INFINITY)', '32');

shouldBe('Math.clz32()', '32');
shouldBe('Math.clz32(undefined)', '32');
shouldBe('Math.clz32(null)', '32');
shouldBe('Math.clz32("WebKit")', '32');
shouldThrow('Math.clz32(Symbol("WebKit"))');
shouldBe('Math.clz32({ webkit: "awesome" })', '32');

// Type conversion.
var objectConvertToString = { toString: function() { return "66"; } };
shouldBe('Math.clz32(objectConvertToString)', '25');

var objectRecordToStringCall = { toStringCallCount: 0, toString: function() { this.toStringCallCount += 1; return "9"; } };
shouldBe('Math.clz32(objectRecordToStringCall)', '28');
shouldBe('objectRecordToStringCall.toStringCallCount', '1');

var objectThrowOnToString = { toString: function() { throw "No!"; } };
shouldThrow('Math.clz32(objectThrowOnToString)');

var objectWithValueOf = { valueOf: function() { return 95014; } };
shouldBe('Math.clz32(objectWithValueOf)', '15');

var objectThrowOnValueOf = { valueOf: function() { throw "Nope!" }, toString: function() { return 5; } };
shouldThrow('Math.clz32(objectThrowOnValueOf)');


var objectRecordValueOfCall = { valueOfCallCount: 0, valueOf: function() { ++this.valueOfCallCount; return 436; } }
shouldBe('Math.clz32(objectRecordValueOfCall)', '23');
shouldBe('objectRecordValueOfCall.valueOfCallCount', '1');

var objectRecordConversionCalls = {
    callList: [],
    toString: function() {
        this.callList.push("toString");
        return "Uh?";
    },
    valueOf: function() {
        this.callList.push("valueOf");
        return 87665;
    }
};
shouldBe('Math.clz32(objectRecordConversionCalls)', '15');
shouldBe('objectRecordConversionCalls.callList.toString()', '"valueOf"');
