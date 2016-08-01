// This test checks the behavior of the String iterator

var testString = "Cocoa,Cappuccino";
var stringIterator = testString[Symbol.iterator]();
var stringIteratorPrototype = stringIterator.__proto__;
var stringIteratorPrototypeNext = stringIteratorPrototype.next;

if (stringIterator.hasOwnProperty('next'))
    throw "next method should exists on %StringIteratorPrototype%";
if (!stringIteratorPrototype.hasOwnProperty('next'))
    throw "next method should exists on %StringIteratorPrototype%";

var iterator = testString[Symbol.iterator]();
var i = 0;
while (true) {
    var {done, value} = iterator.next();
    if (done)
        break;
    if (value !== testString[i])
        throw "Error: bad value: " + value;
    i++;
}

if (testString.length !== i)
    throw "Error: bad value: " + i;

function testSurrogatePair(testString, expected, numberOfElements) {
    if (testString.length !== numberOfElements)
        throw "Error: bad value: " + testString.length;

    var iterator = testString[Symbol.iterator]();
    var i = 0;
    while (true) {
        var {done, value} = iterator.next();
        if (done)
            break;
        if (value !== expected[i])
            throw "Error: bad value: " + value;
        i++;
    }

    if (i !== expected.length)
        throw "Error: bad value: " + i;

    for (var codePoint of testString) {
        if (value !== expected[i])
            throw "Error: bad value: " + value;
    }
}

// "\uD842\uDFB7\u91ce\u5bb6"
var testString = "𠮷野家";
var expected = [
    String.fromCharCode(0xD842, 0xDFB7),
    String.fromCharCode(0x91CE),
    String.fromCharCode(0x5BB6),
];
testSurrogatePair(testString, expected, 4);

var testString = "A\uD842";
var expected = [
    String.fromCharCode(0x0041),
    String.fromCharCode(0xD842),
];
testSurrogatePair(testString, expected, 2);

var testString = "A\uD842A";
var expected = [
    String.fromCharCode(0x0041),
    String.fromCharCode(0xD842),
    String.fromCharCode(0x0041),
];
testSurrogatePair(testString, expected, 3);

var testString = "A\uD842\uDFB7";
var expected = [
    String.fromCharCode(0x0041),
    String.fromCharCode(0xD842, 0xDFB7),
];
testSurrogatePair(testString, expected, 3);

var testString = "\uD842A\uDFB7";
var expected = [
    String.fromCharCode(0xD842),
    String.fromCharCode(0x0041),
    String.fromCharCode(0xDFB7),
];
testSurrogatePair(testString, expected, 3);

var testString = "\uDFB7\uD842A";
var expected = [
    String.fromCharCode(0xDFB7),
    String.fromCharCode(0xD842),
    String.fromCharCode(0x0041),
];
testSurrogatePair(testString, expected, 3);

var string1 = "Cocoa";
var string1Iterator = string1[Symbol.iterator]();
var index = 0;
while (true) {
    var result = stringIteratorPrototypeNext.call(string1Iterator);
    var value = result.value;
    if (result.done) {
        break;
    }
    if (value !== string1[index++])
        throw "Error: bad value: " + value;
}
if (index !== 5)
    throw "Error: bad index: " + index;

function increment(iter) {
    return stringIteratorPrototypeNext.call(iter);
}
var string1 = "Cocoa";
var string2 = "Cocoa";
var string1Iterator = string1[Symbol.iterator]();
var string2Iterator = string2[Symbol.iterator]();
for (var i = 0; i < 3; ++i) {
    var value1 = increment(string1Iterator).value;
    var value2 = increment(string2Iterator).value;
    if (value1 !== value2)
        throw "Error: bad value: " + value1 + " " + value2;
}

var string1 = "Cappuccino";
var string1Iterator = string1[Symbol.iterator]();

var value = string1Iterator.next().value;
if (value !== "C")
    throw "Error: bad value: " + value;
var value = string1Iterator.next().value;
if (value !== "a")
    throw "Error: bad value: " + value;
var value = string1Iterator.next().value;
if (value !== "p")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "p")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "u")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "c")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "c")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "i")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "n")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== "o")
    throw "Error: bad value: " + value;
var value = stringIteratorPrototypeNext.call(string1Iterator).value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var primitives = [
    "string",
    42,
    0.03,
    false,
    true,
    Symbol("Cocoa"),
    null,
    undefined
];
for (var primitive of primitives) {
    var didThrow = null;
    try {
        stringIteratorPrototypeNext.call(primitive);
    } catch (e) {
        didThrow = e;
    }
    if (!didThrow)
        throw "Error: no error thrown";
    var message = 'TypeError: %StringIteratorPrototype%.next requires that |this| be a String Iterator instance';
    if (primitive == null)
        message = 'TypeError: %StringIteratorPrototype%.next requires that |this| not be null or undefined'
    if (String(didThrow) !== message)
        throw "Error: bad error thrown: " + didThrow;
}

var nonRelatedObjects = [
    {},
    [],
    new Date(),
    new Error(),
    Object(Symbol()),
    new String("Cappuccino"),
    new Number(42),
    new Boolean(false),
    function () { },
];
for (var object of nonRelatedObjects) {
    var didThrow = null;
    try {
        stringIteratorPrototypeNext.call(object);
    } catch (e) {
        didThrow = e;
    }
    if (!didThrow)
        throw "Error: no error thrown";
    if (String(didThrow) !== 'TypeError: %StringIteratorPrototype%.next requires that |this| be a String Iterator instance')
        throw "Error: bad error thrown: " + didThrow;
}
