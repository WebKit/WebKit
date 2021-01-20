function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo() {
    return 0;
}

try {
    foo.apply({}, 2n);
    assert(false);
} catch(e) {
    assert(e.message == "second argument to Function.prototype.apply must be an Array-like object (evaluating 'foo.apply({}, 2n)')")
}

