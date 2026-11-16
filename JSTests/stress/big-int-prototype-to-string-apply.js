function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function throwsTypeError(v) {
    try {
        BigInt.prototype.toString.apply(v);
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
}

throwsTypeError(3);
throwsTypeError(3.5);
throwsTypeError("ABC");
throwsTypeError(Symbol("test"));
throwsTypeError({});
throwsTypeError(true);
throwsTypeError([3n]);

