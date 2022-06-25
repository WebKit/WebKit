function assert(a, message) {
    if (!a)
        throw new Error(message);
}

let o = {
    valueOf: function () { throw new Error("Oops"); }
};

try {
    let n = Symbol("3") + o;
    assert(false, message + ": Should throw Error, but executed without exception");
} catch (e) {
    assert(e.message === "Oops","Expected Error('Oops'), got: " + e);
}

try {
    let n = o + Symbol("3");
    assert(false, message + ": Should throw Error, but executed without exception");
} catch (e) {
    assert(e.message === "Oops","Expected Error('Oops'), got: " + e);
}

