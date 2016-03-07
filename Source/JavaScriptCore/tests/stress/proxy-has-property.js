function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

{
    let error = null;
    let target = {
        x: 40
    };

    let handler = {
        get has() {
            error = new Error();
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}

{
    let error = null;
    let target = {
        x: 40
    };

    let handler = {
        has: function() {
            error = new Error();
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}

{
    let target = {
        x: 40
    };

    let called = false;
    let handler = {
        has: function(theTarget, propName) {
            called = true;
            return propName in theTarget;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: true, value: 45});

    let called = false;
    let handler = {
        has: function(theTarget, propName) {
            called = true;
            return propName in theTarget;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: true, value: 45});
    let handler = { };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        if (i % 2)
            handler.has = null;
        else
            handler.has = undefined;
        let result = "x" in proxy;
        assert(result);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: true, value: 45});
    let handler = {
        has: function() {
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(!result);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let handler = {
        has: function() {
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy 'has' must return 'true' for non-configurable properties.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let handler = {
        has: function() {
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy 'has' must return 'true' for non-configurable properties.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let handler = {
        has: function() {
            return null;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy 'has' must return 'true' for non-configurable properties.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let handler = {
        has: function() {
            return undefined;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy 'has' must return 'true' for non-configurable properties.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let handler = {
        has: function() {
            return 0;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "x" in proxy;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy 'has' must return 'true' for non-configurable properties.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let called = false;
    let handler = {
        has: function() {
            called = true;
            return 1;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", { enumerable: true, configurable: false, value: 45});
    let called = false;
    let handler = {
        has: function() {
            called = true;
            return "hello";
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    let called = false;
    let handler = {
        has: function() {
            called = true;
            return true
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let proto = {x: 20};
    let target = Object.create(proto);
    assert(target.__proto__ === proto);
    let called = false;
    let handler = {
        has: function(target, propName) {
            called = true;
            return propName in target;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = {x: 20};
    let handler = {
        has: function(theTarget, propName) {
            assert(theTarget === target);
            called = true;
            return propName in theTarget;
        }
    };
    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy);
    assert(Reflect.getPrototypeOf(obj) === proxy);
    let called = false;

    for (let i = 0; i < 500; i++) {
        let result = "x" in obj;
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let error = null;
    let target = new Proxy({}, {
        getOwnPropertyDescriptor: function() {
            error = new Error();
            throw error;
        }
    });
    let handler = {
        has: function(theTarget, propName) {
            return false;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "foo" in proxy;
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let e1 = null;
    let e2 = null;
    let t1 = {};
    let called1 = false;
    let h1 = {
        has: function(theTarget, propName) {
            called1 = true;
            e1 = new Error;
            throw e1;
            return false;
        }
    };
    let p1 = new Proxy(t1, h1);

    let t2 = {};
    t2.__proto__ = p1;
    let h2 = {
        has: function(theTarget, propName) {
            e2 = new Error;
            throw e2;
            return false;
        }
    };
    let p2 = new Proxy(t2, h2);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            10 in p2;
        } catch(e) {
            assert(e === e2);
            threw = true;
        }
        assert(threw);
        assert(!called1);
    }
}

{
    let e1 = null;
    let e2 = null;
    let t1 = {};
    let called1 = false;
    let h1 = {
        has: function(theTarget, propName) {
            called1 = true;
            e1 = new Error;
            throw e1;
            return false;
        }
    };
    let p1 = new Proxy(t1, h1);

    let t2 = {};
    t2.__proto__ = p1;
    let h2 = {
        has: function(theTarget, propName) {
            e2 = new Error;
            throw e2;
            return false;
        }
    };
    let p2 = new Proxy(t2, h2);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            "foo" in p2;
        } catch(e) {
            assert(e === e2);
            threw = true;
        }
        assert(threw);
        assert(!called1);
    }
}
