function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

// Currently, only "get", "getOwnPropertyDescriptor", and "set" are testable.

{
    let theTarget = [];
    let sawPrivateSymbolAsString = false;
    let handler = {
        get: function(target, propName, proxyArg) {
            if (typeof propName === "string")
                sawPrivateSymbolAsString = propName === "PrivateSymbol.arrayIterationKind";
            return target[propName];
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            proxy[Symbol.iterator]().next.call(proxy);
        } catch(e) {
            // this will throw because we convert private symbols to strings.
            assert(e.message === "%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
            threw = true;
        }
        assert(threw);
        assert(!sawPrivateSymbolAsString);
        sawPrivateSymbolAsString = false;
    }
}

{
    let theTarget = [];
    let sawPrivateSymbolAsString = false;
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            if (typeof propName === "string")
                sawPrivateSymbolAsString = propName === "PrivateSymbol.arrayIterationKind";
            return target[propName];
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            proxy[Symbol.iterator]().next.call(proxy);
        } catch(e) {
            // this will throw because we convert private symbols to strings.
            assert(e.message === "%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
            threw = true;
        }
        assert(threw);
        assert(!sawPrivateSymbolAsString);
        sawPrivateSymbolAsString = false;
    }
}

{
    let theTarget = [1,2,3,4,5];
    let iterator = theTarget[Symbol.iterator]();
    let sawPrivateSymbolAsString = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            if (typeof propName === "string")
                sawPrivateSymbolAsString = propName === "PrivateSymbol.arrayIterationKind";
            return target[propName];
        }
    };

    let proxy = new Proxy(iterator, handler);
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            proxy.next();
        } catch(e) {
            // this will throw because we convert private symbols to strings.
            assert(e.message === "%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
            threw = true;
        }
        assert(!threw);
        assert(!sawPrivateSymbolAsString);
        sawPrivateSymbolAsString = false;
    }
}
