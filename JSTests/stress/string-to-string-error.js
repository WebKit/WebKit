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
    shouldThrow(() => test(object), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(false), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(true), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(42), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(null), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(undefined), `TypeError: String.prototype.toString requires that |this| be a string`);
    shouldThrow(() => test(symbol), `TypeError: String.prototype.toString requires that |this| be a string`);
}

var string = "Hello";
var stringObject = new String(string);
for (var i = 0; i < 1e2; ++i) {
    shouldBe(test(string), string);
    shouldBe(test(stringObject), string);
}
