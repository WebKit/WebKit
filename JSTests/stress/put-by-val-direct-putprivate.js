//@ skip # private field implementation has a bug and this test is failing. Skipping now since private field is not enabled yet.
//@ requireOptions("--usePrivateClassFields=1")

// PrivateField "Put" access should throw if writing to a non-existent PrivateName.
let c, i = 0, threw = false;
class C {
    #x;
    constructor() { return i < 30 ? this : { [Symbol.toStringTag]: "without #x" }; }
    static x(obj) { return obj.#x = i++; }
    get [Symbol.toStringTag]() { return "with #x" }
}

try {
    for (let j = 0; j < 50; ++j) {
        let obj = new C;
        let result = C.x(obj);
        if (result !== j)
            throw new Error(`Expected C.x(${obj}) to be ${j}, but found ${result}`);
    }
} catch (e) {
    threw = true;
    // rval of AssignmentExpression is evaluated before PrivateFieldFind(#x, lref) is performed,
    // thus `i` has already been incremented.
    if (i !== 31 || e.constructor !== TypeError) {
        throw e;
    }
}

if (!threw)
    throw new Error("Expected TypeError, but no exception was thrown");
