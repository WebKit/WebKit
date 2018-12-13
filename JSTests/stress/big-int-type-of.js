//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert(typeof 0n === "bigint");
assert(typeof 1n !== "object");

function typeOf(value)
{
    return typeof value;
}
noInline(typeOf);

var object = {};
var func = function () { };
var bigInt = 1n;
var number = 0;
var string = "String";
var symbol = Symbol("Symbol");

for (var i = 0; i < 1e6; ++i) {
    assert(typeOf(object) === "object");
    assert(typeOf(func) === "function");
    assert(typeOf(bigInt) === "bigint");
    assert(typeOf(number) === "number");
    assert(typeOf(string) === "string");
    assert(typeOf(symbol) === "symbol");
    assert(typeOf(null) === "object");
    assert(typeOf(undefined) === "undefined");
    assert(typeOf(true) === "boolean");
}
