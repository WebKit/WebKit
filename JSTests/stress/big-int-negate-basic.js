// Original tests from https://github.com/tc39/test262/blob/master/test/language/expressions/unary-minus/bigint.js

function assert(a, b, message) {
    if (a !== b)
        throw new Error(message);
}

function assertNotEqual(a, b, message) {
    if (a === b)
        throw new Error(message);
}

assert(-0n, 0n, "-0n === 0n");
assert(-(0n), 0n, "-(0n) === 0n");
assertNotEqual(-1n, 1n, "-1n !== 1n");
assert(-(1n), -1n, "-(1n) === -1n");
assertNotEqual(-(1n), 1n, "-(1n) !== 1n");
assert(-(-1n), 1n, "-(-1n) === 1n");
assertNotEqual(-(-1n), -1n, "-(-1n) !== -1n");
assert(- - 1n, 1n, "- - 1n === 1n");
assertNotEqual(- - 1n, -1n, "- - 1n !== -1n");
assert(-(0x1fffffffffffff01n), -0x1fffffffffffff01n, "-(0x1fffffffffffff01n) === -0x1fffffffffffff01n");
assertNotEqual(-(0x1fffffffffffff01n), 0x1fffffffffffff01n, "-(0x1fffffffffffff01n) !== 0x1fffffffffffff01n");
assertNotEqual(-(0x1fffffffffffff01n), -0x1fffffffffffff00n, "-(0x1fffffffffffff01n) !== -0x1fffffffffffff00n");

// Non-primitive cases

assert(-Object(1n), -1n, "-Object(1n) === -1n");
assertNotEqual(-Object(1n), 1n, "-Object(1n) !== 1n");
assertNotEqual(-Object(1n), Object(-1n), "-Object(1n) !== Object(-1n)");
assert(-Object(-1n), 1n, "-Object(-1n) === 1n");
assertNotEqual(-Object(-1n), -1n, "-Object(-1n) !== -1n");
assertNotEqual(-Object(-1n), Object(1n), "-Object(-1n) !== Object(1n)");

let obj = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    },
    valueOf: function() {
        throw new Error("Should never be called");
    },
    toString: function() {
        throw new Error("Should never be called");
    }
};
assert(-obj, -1n, "@@toPrimitive not called properly");

obj = {
    valueOf: function() {
        return 1n;
    },
    toString: function() {
        throw new Error("Should never be called");
    }
}
assert(-obj, -1n, "valueOf not called properly");

obj = {
    toString: function() {
        return 1n;
    }
};

assert(-obj, -1n, "-{toString: function() { return 1n; }} === -1n");

let x = 1n;
let y = -x;
let z = -y;
assert(x, z, "-(-x) !== z");

