function assert(a, message) {
    if (!a)
        throw new Error(message);
}

function assertThrowTypeError(a, b, message) {
    try {
        let n = a & b;
        assert(false, message + ": Should throw TypeError, but executed without exception");
    } catch (e) {
        assert(e instanceof TypeError, message + ": expected TypeError, got: " + e);
    }
}

assertThrowTypeError(30n, "foo", "BigInt & String");
assertThrowTypeError("bar", 18757382984821n, "String & BigInt");
assertThrowTypeError(30n, Symbol("foo"), "BigInt & Symbol");
assertThrowTypeError(Symbol("bar"), 18757382984821n, "Symbol & BigInt");
assertThrowTypeError(30n, 3320, "BigInt & Int32");
assertThrowTypeError(33256, 18757382984821n, "Int32 & BigInt");
assertThrowTypeError(30n, 0.543, "BigInt & Double");
assertThrowTypeError(230.19293, 18757382984821n, "Double & BigInt");
assertThrowTypeError(30n, NaN, "BigInt & NaN");
assertThrowTypeError(NaN, 18757382984821n, "NaN & BigInt");
assertThrowTypeError(30n, NaN, "BigInt & NaN");
assertThrowTypeError(NaN, 18757382984821n, "NaN & BigInt");
assertThrowTypeError(30n, +Infinity, "BigInt & NaN");
assertThrowTypeError(+Infinity, 18757382984821n, "NaN & BigInt");
assertThrowTypeError(30n, -Infinity, "BigInt & -Infinity");
assertThrowTypeError(-Infinity, 18757382984821n, "-Infinity & BigInt");
assertThrowTypeError(30n, null, "BigInt & null");
assertThrowTypeError(null, 18757382984821n, "null & BigInt");
assertThrowTypeError(30n, undefined, "BigInt & undefined");
assertThrowTypeError(undefined, 18757382984821n, "undefined & BigInt");
assertThrowTypeError(30n, true, "BigInt & true");
assertThrowTypeError(true, 18757382984821n, "true & BigInt");
assertThrowTypeError(30n, false, "BigInt & false");
assertThrowTypeError(false, 18757382984821n, "false & BigInt");

// Error when returning from object

let o = {
    valueOf: function () { return Symbol("Foo"); }
};

assertThrowTypeError(30n, o, "BigInt & Object.valueOf returning Symbol");
assertThrowTypeError(o, 18757382984821n, "Object.valueOf returning Symbol & BigInt");

o = {
    valueOf: function () { return 33256; }
};

assertThrowTypeError(30n, o, "BigInt & Object.valueOf returning Int32");
assertThrowTypeError(o, 18757382984821n, "Object.valueOf returning Int32 & BigInt");

o = {
    valueOf: function () { return 0.453; }
};

assertThrowTypeError(30n, o, "BigInt & Object.valueOf returning Double");
assertThrowTypeError(o, 18757382984821n, "Object.valueOf returning Double & BigInt");

o = {
    toString: function () { return Symbol("Foo"); }
};

assertThrowTypeError(30n, o, "BigInt & Object.toString returning Symbol");
assertThrowTypeError(o, 18757382984821n, "Object.toString returning Symbol & BigInt");

o = {
    toString: function () { return 33256; }
};

assertThrowTypeError(30n, o, "BigInt & Object.toString returning Int32");
assertThrowTypeError(o, 18757382984821n, "Object.toString returning Int32 & BigInt");

o = {
    toString: function () { return 0.453; }
};

assertThrowTypeError(30n, o, "BigInt & Object.toString returning Double");
assertThrowTypeError(o, 18757382984821n, "Object.toString returning Double & BigInt");

o = {
    [Symbol.toPrimitive]: function () { return Symbol("Foo"); }
};

assertThrowTypeError(30n, o, "BigInt & Object.@@toPrimitive returning Symbol");
assertThrowTypeError(o, 18757382984821n, "Object.@@toPrimitive returning Symbol & BigInt");

o = {
    [Symbol.toPrimitive]: function () { return 33256; }
};

assertThrowTypeError(30n, o, "BigInt & Object.@@toPrimitive returning Int32");
assertThrowTypeError(o, 18757382984821n, "Object.@@toPrimitive returning Int32 & BigInt");

o = {
    [Symbol.toPrimitive]: function () { return 0.453; }
};

assertThrowTypeError(30n, o, "BigInt & Object.@@toPrimitive returning Double");
assertThrowTypeError(o, 18757382984821n, "Object.@@toPrimitive returning Double & BigInt");

