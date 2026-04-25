C = class extends Array { }
N = class { }
N[Symbol.species] = function() { throw "this should never be called"; }

function id(x) { return x; }

testFunctions = [
    [Array.prototype.concat, []],
    [Array.prototype.slice, [1,2]],
    [Array.prototype.splice, []],
    [Array.prototype.splice, [0,1]],
    [Array.prototype.map, [id]],
    [Array.prototype.filter, [id]]
];

objProp = Object.defineProperty;

function funcThrows(func, args) {
    try {
        func.call(...args)
        return false;
    } catch (e) {
        return true;
    }
}

function test(testData) {
    "use strict";
    let [protoFunction, args] = testData;
    let foo = new C(10);
    let n = new N();

    // Test non-array ignores constructor.
    objProp(n, "constructor", { value: C });
    let bar = protoFunction.call(n, ...args);
    if (!(bar instanceof Array) || bar instanceof N || bar instanceof C)
        throw Error();

    objProp(foo, "constructor", { value: null });
    if (!funcThrows(protoFunction, [foo, ...args]))
        throw "didn't throw";

    // Test array defaults cases.
    foo = new C(10);

    objProp(C, Symbol.species, { value: undefined, writable: true});
    bar = protoFunction.call(foo, ...args);
    if (!(bar instanceof Array) || bar instanceof C)
        throw Error();

    C[Symbol.species] = null;
    bar = protoFunction.call(foo, ...args);
    if (!(bar instanceof Array) || bar instanceof C)
        throw Error();

    // Test species is custom constructor.
    let called = false;
    function species(...args) {
        called = true;
        return new C(...args);
    }

    C[Symbol.species] = species;
    bar = protoFunction.call(foo, ...args);

    if (!(bar instanceof Array) || !(bar instanceof C) || !called)
        throw Error("failed on " + protoFunction);

    function speciesThrows() {
        throw Error();
    }

    C[Symbol.species] = speciesThrows;
    if (!funcThrows(protoFunction, [foo, ...args]))
        throw "didn't throw";

    C[Symbol.species] = true;
    if (!funcThrows(protoFunction, [foo, ...args]))
        throw "didn't throw";

}

testFunctions.forEach(test);
