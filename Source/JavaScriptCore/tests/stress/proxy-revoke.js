function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}

{
    assert(Proxy.revocable.length === 2);
    assert(Proxy.revocable.name === "revocable");

    {
        let threw = false;
        try {
            new Proxy.revocable;
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy.revocable cannot be constructed. It can only be called");
        }
        assert(threw);
    }

    {
        let threw = false;
        try {
            Proxy.revocable();
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy.revocable needs to be called with two arguments: the target and the handler");
        }
        assert(threw);
    }

    {
        let threw = false;
        try {
            Proxy.revocable({});
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy.revocable needs to be called with two arguments: the target and the handler");
        }
        assert(threw);
    }

    {
        let threw = false;
        try {
            Proxy.revocable({}, null);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'handler' should be an Object");
        }
        assert(threw);
    }

    {
        let threw = false;
        try {
            Proxy.revocable(null, {});
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'target' should be an Object");
        }
        assert(threw);
    }

    {
        let threw = false;
        try {
            Proxy.revocable({}, {}, {}); // It's okay to call with > 2 arguments.
        } catch(e) {
            threw = true;
        }
        assert(!threw);
    }

    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            let {revoke} =  Proxy.revocable({}, {}); // It's okay to call with > 2 arguments.
            new revoke;
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: function is not a constructor (evaluating 'new revoke')");
        }
        assert(threw);
    }

    for (let i = 0; i < 500; i++) {
        function foo() {
            let threw = false;
            let {proxy, revoke} = Proxy.revocable({}, {});
            revoke();
            try {
                new Proxy(proxy, {});
            } catch(e) {
                threw = true;
                assert(e.toString() === "TypeError: If a Proxy's handler is another Proxy object, the other Proxy should not have been revoked");
            }
            assert(threw);
        }
        foo();
    }
}

function callAllHandlers(proxy) {
    Reflect.getPrototypeOf(proxy);
    Reflect.setPrototypeOf(proxy, null);
    Reflect.isExtensible(proxy);
    Reflect.preventExtensions(proxy);
    Reflect.getOwnPropertyDescriptor(proxy, "x")
    Reflect.has(proxy, "x")
    Reflect.get(proxy, "x")
    proxy["x"] = 20; // Reflect.set
    Reflect.deleteProperty(proxy, "x");
    Reflect.defineProperty(proxy, "x", {value: 40, enumerable: true, configurable: true});
    Reflect.ownKeys(proxy);
    Reflect.apply(proxy, this, []);
    Reflect.construct(proxy, []);
}

function shouldThrowNullHandler(f) {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Proxy has already been revoked. No more operations are allowed to be performed on it");
    }
    assert(threw);
}

function allHandlersShouldThrow(proxy) {
    shouldThrowNullHandler(() => Reflect.getPrototypeOf(proxy));
    shouldThrowNullHandler(() => Reflect.setPrototypeOf(proxy, null));
    shouldThrowNullHandler(() => Reflect.isExtensible(proxy));
    shouldThrowNullHandler(() => Reflect.preventExtensions(proxy));
    shouldThrowNullHandler(() => Reflect.getOwnPropertyDescriptor(proxy, "x"));
    shouldThrowNullHandler(() => Reflect.has(proxy, "x"));
    shouldThrowNullHandler(() => Reflect.get(proxy, "x"));
    shouldThrowNullHandler(() => proxy["x"] = 20); // Reflect.set
    shouldThrowNullHandler(() => Reflect.deleteProperty(proxy, "x"));
    shouldThrowNullHandler(() => Reflect.defineProperty(proxy, "x", {value: 40, enumerable: true, configurable: true}));
    shouldThrowNullHandler(() => Reflect.ownKeys(proxy));
    shouldThrowNullHandler(() => Reflect.apply(proxy, this, []));
    shouldThrowNullHandler(() => Reflect.construct(proxy, []));
}

const traps = [
    "getPrototypeOf",
    "setPrototypeOf",
    "isExtensible",
    "preventExtensions",
    "getOwnPropertyDescriptor",
    "has",
    "get",
    "set",
    "deleteProperty",
    "defineProperty",
    "ownKeys",
    "apply",
    "construct"
];

for (let i = 0; i < 500; i++) {
    let handler = {};
    let count = 0;
    let trapsCalled = new Set;
    for (let trap of traps) {
        let func;
        if (trap !== "set") {
            func = `(function ${trap}(...args) { count++; trapsCalled.add(${"'" + trap + "'"}); return Reflect.${trap}(...args); })`;
        } else {
            func = `(function ${trap}(proxy, prop, v) { trapsCalled.add(${"'" + trap + "'"}); count++; proxy[prop] = v; })`;
        }
        handler[trap] = eval(func);
    }


    let {proxy, revoke} = Proxy.revocable(function(){}, handler); // To make callable and constructible true.
    callAllHandlers(proxy);
    assert(count >= traps.length);
    for (let trap of traps) {
        let result = trapsCalled.has(trap);
        assert(result);
    }

    revoke();
    allHandlersShouldThrow(proxy);

    for (let i = 0; i < 50; i++)
        revoke(); // Repeatedly calling revoke is OK.
}
