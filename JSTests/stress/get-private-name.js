// FIXME: //@ requireOptions("--usePrivateClassFields=1") --- Run this in all variants once https://bugs.webkit.org/show_bug.cgi?id=212781 is fixed
//@ runNoJIT("--usePrivateClassFields=1")
//@ runNoLLInt("--usePrivateClassFields=1")

// GetPrivateName should throw when the receiver does not have the requested private property
let i, threw = false;
class C {
    #x = i;
    constructor() { if (i === 30) return { [Symbol.toStringTag]: "without #x"}; }
    static x(obj) { return obj.#x; }
    get [Symbol.toStringTag]() { return "with #x"; }
}

try {
    for (i = 0; i < 50; ++i) {
        let c = new C;
        let result = C.x(c);
        if (result !== i)
            throw new Error(`Expected C.x(${c}) to be ${i}, but found ${result}`);
    }
} catch (e) {
    threw = true;
    if (i !== 30 || e.constructor !== TypeError) {
        throw e;
    }
}

if (!threw)
    throw new Error("Expected TypeError, but no exception was thrown");
