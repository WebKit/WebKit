C = class extends Promise { }
N = class { }
N[Symbol.species] = function() { throw "this should never be called"; }

function id(x) { return x; }

testFunctions = [
    [Promise.prototype.then, [id]]
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

function makeC() {
    return new C(function(resolve) { resolve(1); });
}

function test(testData) {
    "use strict";
    let [protoFunction, args] = testData;
    let foo = makeC()
    let n = new N();

    // Test promise defaults cases.
    foo = makeC();

    objProp(C, Symbol.species, { value: undefined, writable: true});
    let bar = protoFunction.call(foo, ...args);
    if (!(bar instanceof Promise) || bar instanceof C)
        throw Error();

    C[Symbol.species] = null;
    bar = protoFunction.call(foo, ...args);
    if (!(bar instanceof Promise) || bar instanceof C)
        throw Error();

    // Test species is custom constructor.
    let called = false;
    function species() {
        called = true;
        return new C(...arguments);
    }

    C[Symbol.species] = species;
    bar = protoFunction.call(foo, ...args);

    if (!(bar instanceof Promise) || !(bar instanceof C) || !called)
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
