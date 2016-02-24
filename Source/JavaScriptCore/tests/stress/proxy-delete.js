function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

{
    let target = {x: 20};
    let error = null;
    let handler = {
        get deleteProperty() {
            error = new Error;
            throw error;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            delete proxy.x;
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {x: 20};
    let error = null;
    let handler = {
        deleteProperty: function() {
            error = new Error;
            throw error;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            delete proxy.x;
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {x: 20};
    let error = null;
    let handler = {
        deleteProperty: 45
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            delete proxy.x;
        } catch(e) {
            assert(e.toString() === "TypeError: 'deleteProperty' property of a Proxy's handler should be callable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    Object.defineProperty(target, "x", {
        configurable: false,
        value: 25
    });
    let error = null;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            delete theTarget[propName];
            return true;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            delete proxy.x;
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy handler's 'deleteProperty' method should return false when the target's property is not configurable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        deleteProperty: null
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target.x = i;
        assert(proxy.x === i);
        let result = delete proxy.x;
        assert(result);
        assert(proxy.x === undefined);
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        deleteProperty: undefined
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target[i] = i;
        assert(proxy[i] === i);
        let result = delete proxy[i];
        assert(result);
        assert(proxy[i] === undefined);
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            assert(theTarget === target);
            assert(propName === "x");
            return delete theTarget[propName];
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target.x = i;
        assert(proxy.x === i);
        let result = delete proxy.x;
        assert(result);
        assert(proxy.x === undefined);
        assert(target.x === undefined);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            assert(theTarget === target);
            assert(propName === "x");
            return delete theTarget[propName];
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target.x = i;
        assert(proxy.x === i);
        let result = delete proxy["x"];
        assert(result);
        assert(proxy.x === undefined);
        assert(target.x === undefined);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            assert(theTarget === target);
            return delete theTarget[propName];
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target[i] = i;
        assert(proxy[i] === i);
        let result = delete proxy[i];
        assert(result);
        assert(proxy[i] === undefined);
        assert(target[i] === undefined);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            assert(theTarget === target);
            delete theTarget[propName];
            return false; // We're liars.
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target[i] = i;
        assert(proxy[i] === i);
        let result = delete proxy[i];
        assert(!result);
        assert(proxy[i] === undefined);
        assert(target[i] === undefined);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let error = null;
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            return delete theTarget[propName];
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        Object.defineProperty(target, "x", {
            configurable: true,
            writable: false,
            value: 25
        });
        target.x = 30;
        assert(target.x === 25);
        assert(proxy.x === 25);
        delete proxy.x;
        assert(target.x === undefined);
        assert(proxy.x === undefined);
        assert(!("x" in proxy));
        assert(!("x" in target));
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let error = null;
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: false,
        value: 25
    });
    let called = false;
    let handler = {
        deleteProperty: function(theTarget, propName) {
            called = true;
            return delete theTarget[propName];
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        target.x = 30;
        assert(target.x === 25);
        assert(proxy.x === 25);
        let result = delete proxy.x;
        assert(!result);
        assert(called);
        called = false;
    }
}
