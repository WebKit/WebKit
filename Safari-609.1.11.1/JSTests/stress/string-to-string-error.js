function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var toString = String.prototype.toString;
function test(string)
{
    return toString.call(string);
}
noInline(test);

var object = {};
var symbol = Symbol("Cocoa");
for (var i = 0; i < 3e3; ++i) {
    shouldThrow(() => test(object), `TypeError: Type error`);
    shouldThrow(() => test(false), `TypeError: Type error`);
    shouldThrow(() => test(true), `TypeError: Type error`);
    shouldThrow(() => test(42), `TypeError: Type error`);
    shouldThrow(() => test(null), `TypeError: Type error`);
    shouldThrow(() => test(undefined), `TypeError: Type error`);
    shouldThrow(() => test(symbol), `TypeError: Type error`);
}

var string = "Hello";
var stringObject = new String(string);
for (var i = 0; i < 1e2; ++i) {
    shouldBe(test(string), string);
    shouldBe(test(stringObject), string);
}
