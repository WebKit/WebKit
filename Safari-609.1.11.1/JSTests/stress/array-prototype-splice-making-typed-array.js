function assert(b) {
    if (!b)
        throw new Error("Bad assertion!")
}

function test(f, n = 4) {
    for (let i = 0; i < n; i++)
        f();
}

test(function() {
    // This should not crash.
    let x = [1,2,3,4,5];
    x.constructor = Uint8Array;
    delete x[2];
    assert(!(2 in x));
    let err = null;
    try {
        let removed = x.splice(1,3);
        assert(removed instanceof Uint8Array);
        assert(removed.length === 3);
        assert(removed[0] === 2);
        assert(removed[1] === 0);
        assert(removed[2] === 4);
    } catch(e) {
        err = e;
    }
    assert(err.toString() === "TypeError: Attempting to configure non-configurable property on a typed array at index: 0");

    assert(x instanceof Array);
    assert(x.length === 5);
    assert(x[0] === 1);
    assert(x[1] === 2);
    assert(x[2] === undefined);
    assert(x[3] === 4);
    assert(x[4] === 5);
});

test(function() {
    let x = [1,2,3,4,5];
    x.constructor = Uint8Array;
    delete x[2];
    assert(!(2 in x));
    Object.defineProperty(Uint8Array, Symbol.species, {value: null});
    assert(Uint8Array[Symbol.species] === null);
    x = new Proxy(x, {
        get(target, property, receiver) {
            if (parseInt(property, 10))
                assert(property !== "2");
            return Reflect.get(target, property, receiver);
        }
    });

    let removed = x.splice(1,3);
    assert(removed instanceof Array); // We shouldn't make a TypedArray here because Symbol.species is null.
    assert(removed.length === 3);
    assert(removed[0] === 2);
    assert(removed[1] === undefined);
    assert(!(1 in removed));
    assert(removed[2] === 4);

    assert(x instanceof Array);
    assert(x.length === 2);
    assert(x[0] === 1);
    assert(x[1] === 5);
});
