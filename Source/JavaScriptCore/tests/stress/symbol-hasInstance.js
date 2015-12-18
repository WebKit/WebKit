// This file tests the functionality of Symbol.hasInstance.


// Test a custom Symbol.hasInstance on a function object.
function Constructor(x) {}
foo = new Constructor();

if (!(foo instanceof Constructor))
    throw "should be instanceof";

Object.defineProperty(Constructor, Symbol.hasInstance, {value: function(value) {
    if (this !== Constructor)
        throw "|this| should be Constructor";
    if (value !== foo)
        throw "first argument should be foo";
    return false;
} });


if (foo instanceof Constructor)
    throw "should not be instanceof";


// Test Symbol.hasInstance on an ordinary object.
ObjectClass = {}
ObjectClass[Symbol.hasInstance] = function (value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
}

if (!(foo instanceof ObjectClass))
    throw "foo should be an instanceof ObjectClass";

if (!(Constructor instanceof ObjectClass))
    throw "Constructor should be an instanceof ObjectClass";

NumberClass = {}
NumberClass[Symbol.hasInstance] = function (value) {
    return typeof value === "number";
}

if (!(1 instanceof NumberClass))
    throw "1 should be an instanceof NumberClass";

if (foo instanceof NumberClass)
    throw "foo should be an instanceof NumberClass";


// Test the Function.prototype[Symbol.hasInstance] works when actually called.
descriptor = Object.getOwnPropertyDescriptor(Function.prototype, Symbol.hasInstance);
if (descriptor.writable !== false || descriptor.configurable !== false || descriptor.enumerable !== false)
    throw "Function.prototype[Symbol.hasInstance] has a bad descriptor";

if (!Function.prototype[Symbol.hasInstance].call(Constructor, foo))
    throw "Function.prototype[Symbol.hasInstance] should claim that foo is an instanceof Constructor";
