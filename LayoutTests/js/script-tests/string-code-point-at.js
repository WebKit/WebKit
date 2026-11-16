description("Test the basic behaviors of String.codePointAt");

shouldBe('String.codePointAt', 'undefined');

shouldBeEqualToString('typeof String.prototype.codePointAt', 'function');

// Function properties.
shouldBe('String.prototype.codePointAt.length', '1');
shouldBeEqualToString('String.prototype.codePointAt.name', 'codePointAt')
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "codePointAt").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "codePointAt").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "codePointAt").writable', 'true');

// The function should only be on the prototype chain, not on the object themselves.
shouldBeFalse('"foo".hasOwnProperty("codePointAt")');
shouldBeFalse('(new String("bar")).hasOwnProperty("codePointAt")');

// Some simple cases.
shouldBe('"".codePointAt(0)', 'undefined');
shouldBe('"".codePointAt(1)', 'undefined');

shouldBe('"Été".codePointAt(0)', '201');
shouldBe('"Été".codePointAt(1)', '116');
shouldBe('"Été".codePointAt(2)', '233');
shouldBe('"Été".codePointAt(3)', 'undefined');

shouldBe('"ウェブキット".codePointAt(0)', '12454');
shouldBe('"ウェブキット".codePointAt(1)', '12455');
shouldBe('"ウェブキット".codePointAt(2)', '12502');
shouldBe('"ウェブキット".codePointAt(3)', '12461');
shouldBe('"ウェブキット".codePointAt(4)', '12483');
shouldBe('"ウェブキット".codePointAt(5)', '12488');
shouldBe('"ウェブキット".codePointAt(6)', 'undefined');

// Object coercion.
shouldThrow('"".codePointAt.call(null, 0)');
shouldThrow('"".codePointAt.call(undefined, 0)');
shouldBe('"".codePointAt.call(0, 0)', '48');
shouldBe('"".codePointAt.call(Math.PI, 0)', '51');
shouldBe('"".codePointAt.call(Math.PI, 1)', '46');
shouldBe('"".codePointAt.call(Math.PI, 3)', '52');
shouldBe('"".codePointAt.call(true, 3)', '101');
shouldBe('"".codePointAt.call(false, 3)', '115');
shouldBe('"".codePointAt.call(new Object, 3)', '106');
shouldThrow('"".codePointAt.call(Symbol("WebKit"), 3)');

// toString.
var objectWithCustomToString = { toString: function() { return "ø"; } };
shouldBe('"".codePointAt.call(objectWithCustomToString, 0)', '248');

var objectThrowingOnToString = { toString: function() { throw "Hehe"; } };
shouldThrow('"".codePointAt.call(objectThrowingOnToString, 0)');

var objectCountingToString = { counter: 0, toString: function() { ++this.counter; return this.counter; } };
shouldBe('"".codePointAt.call(objectCountingToString, 0)', '49');
shouldBe('objectCountingToString.counter', '1');

// ToNumber.
var objectWithCustomValueOf = { toString: function() { return "5"; }, valueOf: function() { return 1; } };
shouldBe('"abcde".codePointAt(objectWithCustomValueOf)', '98');

// The second object is never converted to number if the first object did not convert to string.
var objectRecordsValueOf = { valueOfEvaluated: false, valueOf: function() { this.valueOfEvaluated = true; return 1; } }
shouldThrow('"".codePointAt.call(null, objectRecordsValueOf)');
shouldThrow('"".codePointAt.call(undefined, objectRecordsValueOf)');
shouldThrow('"".codePointAt.call(Symbol("WebKit"), objectRecordsValueOf)');
shouldThrow('"".codePointAt.call(objectThrowingOnToString, objectRecordsValueOf)');
shouldBeFalse('objectRecordsValueOf.valueOfEvaluated');

// Evaluation order.
var evaluationOrderRecorder = {
    methodsCalled: [],
    toString: function() { this.methodsCalled.push("toString"); return "foobar"; },
    valueOf: function() { this.methodsCalled.push("valueOf"); return 5; }
}
shouldBe('"".codePointAt.call(evaluationOrderRecorder, evaluationOrderRecorder)', '114');
shouldBeEqualToString('evaluationOrderRecorder.methodsCalled.toString()', 'toString,valueOf');

// Weird positions.
shouldBe('"abc".codePointAt(NaN)', '97');
shouldBe('"abc".codePointAt(-0)', '97');
shouldBe('"abc".codePointAt(-0.0)', '97');
shouldBe('"abc".codePointAt(-0.05)', '97');
shouldBe('"abc".codePointAt(-0.999)', '97');
shouldBe('"abc".codePointAt(0.4)', '97');
shouldBe('"abc".codePointAt(0.9)', '97');
shouldBe('"abc".codePointAt(2.9999)', '99');

// Out of bound positions.
shouldBe('"abc".codePointAt(-1)', 'undefined');
shouldBe('"abc".codePointAt(4)', 'undefined');
shouldBe('var str = "abc"; str.codePointAt(str.length)', 'undefined');
shouldBe('"abc".codePointAt(4.1)', 'undefined');
shouldBe('"abc".codePointAt(Number.POSITIVE_INFINITY)', 'undefined');
shouldBe('"abc".codePointAt(Number.NEGATIVE_INFINITY)', 'undefined');

// Non-number as positions.
shouldBe('"abc".codePointAt(null)', '97');
shouldBe('"abc".codePointAt(undefined)', '97');
shouldBe('"abc".codePointAt("")', '97');
shouldBe('"abc".codePointAt("WebKit!")', '97');
shouldBe('"abc".codePointAt(new Object)', '97');
shouldThrow('"abc".codePointAt(Symbol("WebKit"))');

// The following are using special test functions because of limitations of WebKitTestRunner when handling strings with invalid codepoints.
// When transfering the text of a test, WebKitTestRunner converts it to a UTF-8 C String. Not all invalid code point can be represented.

// If first < 0xD800 or first > 0xDBFF or position+1 = size, return first.
function testLeadSurrogateOutOfBounds()
{
    return ("\uD7FF\uDC00".codePointAt(0) === 0xd7ff && "\uD7FF\uDC00".codePointAt(1) === 0xdc00 && "\uD7FF\uDC00".codePointAt(2) === undefined
            && "\uDC00\uDC00".codePointAt(0) === 0xdc00 && "\uDC00\uDC00".codePointAt(1) === 0xdc00 && "\uDC00\uDC00".codePointAt(2) === undefined)
}
shouldBeTrue("testLeadSurrogateOutOfBounds()");

function testLeadSurrogateAsLastCharacter()
{
    return "abc\uD800".codePointAt(3) === 0xd800;
}
shouldBeTrue("testLeadSurrogateAsLastCharacter()");

// If second < 0xDC00 or second > 0xDFFF, return first.
function testTrailSurrogateOutOfbounds()
{
    return ("\uD800\uDBFF".codePointAt(0) === 0xd800 && "\uD800\uDBFF".codePointAt(1) === 0xdbff && "\uD800\uDBFF".codePointAt(2) === undefined
            && "\uD800\uE000".codePointAt(0) === 0xd800 && "\uD800\uE000".codePointAt(1) === 0xe000 && "\uD800\uE000".codePointAt(2) === undefined)
}
shouldBeTrue("testTrailSurrogateOutOfbounds()");

// Null in a string.
function testAccessNullInString()
{
    return "a\u0000b".codePointAt(0) === 97 && "a\u0000b".codePointAt(1) === 0 && "a\u0000b".codePointAt(2) === 98 && "a\u0000b".codePointAt(3) === undefined;
}
shouldBeTrue("testAccessNullInString()");


// Normal combinations of surrogates.
function testNormalCombinationOfSurrogates()
{
    return "\uD800\uDC00".codePointAt(0) === 65536 && "\uD800\uDC00".codePointAt(1) === 56320 && "\uD800\uDC00".codePointAt(2) === undefined;
}
shouldBeTrue("testNormalCombinationOfSurrogates()");
