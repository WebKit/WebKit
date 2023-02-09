function shouldThrow(fn, expectedError) {
    let errorThrown = false;
    try {
        fn();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

const callableGetTrapError = "TypeError: 'get' property of a Proxy's handler object should be callable";

shouldThrow(() => {
    var handler = {get: null};
    var proxy = new Proxy({foo: 1}, handler);

    for (var i = 0; i < 1e7; ++i) {
        var foo = proxy.foo;
        if (i === 1e7 / 2)
            handler.get = 1;
    }
}, callableGetTrapError);

shouldThrow(() => {
    var handler = {get: undefined};
    var proxy = new Proxy({foo: 1}, handler);

    for (var i = 0; i < 1e7; ++i) {
        var bar = proxy.bar;
        if (i === 1e7 / 2)
            handler.get = "foo";
    }
}, callableGetTrapError);

shouldThrow(() => {
    var handler = {get: null};
    var proxy = new Proxy({foo: 1}, handler);

    for (var i = 0; i < 1e7; ++i) {
        var foo = proxy.foo;
        if (i === 1e7 / 2)
            handler.get = {};
    }
}, callableGetTrapError);

shouldThrow(() => {
    var handler = {get: undefined};
    var proxy = new Proxy(() => {}, handler);

    for (var i = 0; i < 1e7; ++i) {
        var bar = proxy.bar;
        if (i === 1e7 / 2)
            handler.get = [];
    }
}, callableGetTrapError);

