function assert(a, message) {
    if (!a)
        throw new Error(message);
}

function assertThrowTypeError(a, b, message) {
    try {
        let n = a - b;
        assert(false, message + ": Should throw TypeError, but executed without exception");
    } catch (e) {
        assert(e instanceof TypeError, message + ": expected TypeError, got: " + e);
    }
}

let o = {
    valueOf: function () { throw new Error("Oops"); }
};

assertThrowTypeError(Symbol("3"), o, "Symbol + Object should throw TypeError");

try {
    let n = o - Symbol("3");
    assert(false, message + ": Should throw Error, but executed without exception");
} catch (e) {
    assert(e.message === "Oops","Expected Error('Oops'), got: " + e);
}

