function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

assert(Proxy instanceof Function);
assert(Proxy.length === 2);
assert(Proxy.prototype === undefined);

{
    for (let i = 0; i < 100; i++)
        assert((new Proxy({}, {})).__proto__ === Object.prototype);
}

{
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            new Proxy({}, 20);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'handler' should be an Object");
        }
        assert(threw);
    }
}

{
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            new Proxy({}, "");
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'handler' should be an Object");
        }
        assert(threw);
    }
}

{
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            new Proxy(20, {});
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'target' should be an Object");
        }
        assert(threw);
    }
}

{
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            new Proxy("", {});
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: A Proxy's 'target' should be an Object");
        }
        assert(threw);
    }
}

{
    // When we call Proxy it should throw
    for (let i = 0; i < 100; i++) {
        let threw = false;
        try {
            Proxy({}, {});
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: calling Proxy constructor without new is invalid");
        }
        assert(threw === true);
    }

    let theTarget = {
        x: 30
    };

    let handler = {
        get: function(target, propName, proxyArg) {
            assert(target === theTarget);
            assert(proxyArg === proxy);
            if (propName === "y")
                return 45;
            assert(propName === "x");
            return target[propName];
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 1000; i++) {
        assert(proxy.x === 30);
        assert(proxy.y === 45);
        assert(proxy["x"] === 30);
        assert(proxy["y"] === 45);
    }

}

{
    let handler = {get: null};
    let target = {x: 20};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            if (i % 2)
                proxy.foo;
            else
                proxy["foo"];
        } catch(e) {
            threw = true;
        }
        assert(!threw);
    }
}

{
    let handler = {get: {}};
    let target = {x: 20};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            if (i % 2)
                proxy.foo;
            else
                proxy["foo"];
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: 'get' property of a Proxy's handler object should be callable");
        }
        assert(threw);
    }
}

{
    let handler = {};
    let target = {x: 20};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.x === 20);
        assert(proxy.y === undefined);
    }
}

{
    let handler = {};
    let target = [1, 2, 3];
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy[0] === 1);
        assert(proxy[1] === 2);
        assert(proxy[2] === 3);
    }
}

{
    let theTarget = [1, 2, 3];
    let handler = {
        get: function(target, propName, proxyArg) {
            switch (propName) {
            case "0":
            case "1":
                return 100;
            case "2":
            case "length":
                return target[propName];
            }
            assert(false);
        }
    };
    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy[0] === 100);
        assert(proxy[1] === 100);
        assert(proxy[2] === 3);
        assert(proxy.length === 3);
        assert(proxy["length"] === 3);
    }
}

{
    let wasCalled = false;
    let theTarget = {
        get x() {
            wasCalled = true;
            return 25;
        }
    };
    let j = 0;
    let handler = {
        get: function(target, propName, proxyArg) {
            assert(target === theTarget);
            let x = j++;
            if (x % 2)
                return target[propName];
            else
                return "hello";
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        if (i % 2)
            assert(proxy.x === 25);
        else
            assert(proxy.x === "hello");
            
    }

    assert(wasCalled);
}

{
    let theTarget = {
        x: 40
    };
    let handler = {
        get: function(target, propName, proxyArg) {
            return 30;
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.x === 30);
    }
    handler.get = undefined;
    for (let i = 0; i < 500; i++) {
        assert(proxy.x === 40);
    }
}

{
    let error = null;
    let theTarget = new Proxy({}, {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            error = new Error("hello!")
            throw error;
        }
    });

    let handler = {
        get: function(target, propName, proxyArg) {
            return 30;
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        try {
            proxy.x;
        } catch(e) {
            assert(e === error);
        }
    }
}

{
    let field = Symbol();
    let theTarget = {
        [field]: 40
    };
    let handler = {
        get: function(target, propName, proxyArg) {
            assert(propName === field);
            return target[field];
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy[field] === 40);
    }
}

{
    let prop = Symbol();
    let theTarget = { };
    Object.defineProperty(theTarget, prop, {
        enumerable: true,
        configurable: true
    });
    let called = false;
    let handler = {
        getOwnPropertyDescriptor: function(target, propName) {
            assert(prop === propName);
            called = true;
            return {
                enumerable: true,
                configurable: true
            };
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 100; i++) {
        let pDesc = Object.getOwnPropertyDescriptor(proxy, prop);
        assert(pDesc.configurable);
        assert(pDesc.enumerable);
        assert(called);
        called = false;
    }
}

{
    let prop = Symbol();
    let theTarget = { };
    Object.defineProperty(theTarget, prop, {
        enumerable: true,
        configurable: true
    });
    let called = false;
    let handler = {
        has: function(target, propName) {
            assert(prop === propName);
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 100; i++) {
        let result = prop in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}
