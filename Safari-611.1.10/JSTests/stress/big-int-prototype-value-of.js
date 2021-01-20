function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertThrowTypeError(input) {
    try {
        let n = BigInt.prototype.valueOf(input);
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
}

assertThrowTypeError(10);
assertThrowTypeError("abc");
assertThrowTypeError(Symbol("a"));
assertThrowTypeError(10.5);
assertThrowTypeError({});

