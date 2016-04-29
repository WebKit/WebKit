function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var bf = (function(){}).bind();
Object.defineProperty(bf, "prototype", {
    get() { throw Error("OK") }
});

[
    Array,
    Date,
    Boolean,
    Function,
    Number,
    String,
    RegExp,
    Error,
    Uint8Array,
    ArrayBuffer,
    Promise,
    Map,
    WeakMap,
    Set,
    WeakSet,
].forEach(function (constructor) {
    shouldThrow(() => {
        Reflect.construct(constructor, [], bf);
    }, `Error: OK`);
});

// Proxy constructor is not aware of new.target.
Reflect.construct(Proxy, [{}, {}], bf);
