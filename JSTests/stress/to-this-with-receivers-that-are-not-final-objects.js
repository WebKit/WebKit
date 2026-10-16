// Semantics of |this| conversion for receivers of many kinds, monomorphic and mixed at one function.
function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + String(expected) + " but got " + String(actual));
}

function sloppyThis() { return this; }
function strictThis() { "use strict"; return this; }
function sloppyType() { return typeof this; }
function strictType() { "use strict"; return typeof this; }
noInline(sloppyThis);
noInline(strictThis);
noInline(sloppyType);
noInline(strictType);

var globalThisValue = this;
var array = [1, 2];
var fn = function () { };
var map = new Map;
var plain = { a: 1 };
var plain2 = { b: 1 };
var symbol = Symbol("s");
var date = new Date(0);
var regexp = /a/;
var typed = new Uint8Array(2);
function* gen() { yield 1; }
var generator = gen();
var proxy = new Proxy({}, {});
var receivers = [array, fn, map, plain, plain2, date, regexp, typed, generator, proxy];

function callFromScope() {
    // An unqualified call: the callee must not see a scope object as |this|.
    let captured = 1;
    function inner() { return captured; }
    return [sloppyThis(), strictThis(), inner()];
}
noInline(callFromScope);

function callInWith(object) {
    with (object) {
        return [method(), strictMethod()];
    }
}
noInline(callInWith);
var withObject = {
    method() { return this; },
    strictMethod() { "use strict"; return this; },
};

function monomorphicArray() { "use strict"; return this.length; }
function monomorphicFunction() { return this.name; }
function named() { }
noInline(monomorphicArray);
noInline(monomorphicFunction);

for (var i = 0; i < testLoopCount; ++i) {
    for (var receiver of receivers) {
        shouldBe(sloppyThis.call(receiver), receiver, "sloppy object receiver");
        shouldBe(strictThis.call(receiver), receiver, "strict object receiver");
    }

    shouldBe(strictThis.call("str"), "str", "strict string");
    shouldBe(strictType.call("str"), "string", "strict string type");
    shouldBe(sloppyType.call("str"), "object", "sloppy string type");
    shouldBe(String(sloppyThis.call("str")), "str", "sloppy string wrapper");
    shouldBe(sloppyThis.call("str") instanceof String, true, "sloppy string is wrapped");

    shouldBe(strictThis.call(symbol), symbol, "strict symbol");
    shouldBe(sloppyType.call(symbol), "object", "sloppy symbol type");
    shouldBe(strictThis.call(10n), 10n, "strict bigint");
    shouldBe(sloppyType.call(10n), "object", "sloppy bigint type");
    shouldBe(strictThis.call(1.5), 1.5, "strict number");
    shouldBe(sloppyType.call(1.5), "object", "sloppy number type");
    shouldBe(strictThis.call(true), true, "strict boolean");
    shouldBe(sloppyType.call(false), "object", "sloppy boolean type");

    shouldBe(strictThis.call(undefined), undefined, "strict undefined");
    shouldBe(strictThis.call(null), null, "strict null");
    shouldBe(sloppyThis.call(undefined), globalThisValue, "sloppy undefined");
    shouldBe(sloppyThis.call(null), globalThisValue, "sloppy null");
    shouldBe(sloppyThis.call(globalThisValue), globalThisValue, "sloppy global");
    shouldBe(strictThis.call(globalThisValue), globalThisValue, "strict global");

    var fromScope = callFromScope();
    shouldBe(fromScope[0], globalThisValue, "sloppy call from a scope");
    shouldBe(fromScope[1], undefined, "strict call from a scope");
    shouldBe(fromScope[2], 1, "inner call");

    var fromWith = callInWith(withObject);
    shouldBe(fromWith[0], withObject, "sloppy call in with");
    shouldBe(fromWith[1], withObject, "strict call in with");

    shouldBe(monomorphicArray.call(array), 2, "array receiver only");
    shouldBe(monomorphicFunction.call(named), "named", "function receiver only");
}
