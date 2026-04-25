function assert(b) {
    if (!b)
        throw new Error;
}

function readPrototype(f) {
    return f.prototype;
}
noInline(readPrototype);

{
    let f1 = function () { };
    let f2 = () => undefined;
    for (let i = 0; i < 100; ++i) {
        assert(!f2.hasOwnProperty("prototype"));
        assert(f1.hasOwnProperty("prototype"));
    }

    for (let i = 0; i < 100; ++i)
        assert(readPrototype(f2) === undefined);
    assert(readPrototype(f1) !== undefined);
    assert(readPrototype(f1) === f1.prototype);
}
