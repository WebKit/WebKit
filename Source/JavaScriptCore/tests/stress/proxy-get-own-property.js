function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

{
    let target = {x: 20};
    let called = false;
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            called = true;
            assert(theTarget === target);
            assert(propName === "x");
            return undefined;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(Object.getOwnPropertyDescriptor(proxy, "x") === undefined);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            return 25;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            assert(e.toString() === "TypeError: result of 'getOwnPropertyDescriptor' call should either be an Object or undefined.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: false
    });
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            assert(theTarget === target);
            assert(propName === "x");
            return undefined;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            assert(e.toString() === "TypeError: When the result of 'getOwnPropertyDescriptor' is undefined the target must be configurable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: true
    });
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            assert(theTarget === target);
            assert(propName === "x");
            return {configurable: false};
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            assert(e.toString() === "TypeError: Result from 'getOwnPropertyDescriptor' can't be non-configurable when the 'target' doesn't have it as an own property or if it is a configurable own property on 'target'.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    Object.defineProperty(target, "x", {
        enumerable: false,
        configurable: false
    });
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propName) {
            assert(theTarget === target);
            assert(propName === "x");
            return {enumerable: true};
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            assert(e.toString() === "TypeError: Result from 'getOwnPropertyDescriptor' fails the IsCompatiblePropertyDescriptor test.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let handler = {
        getOwnPropertyDescriptor: 45
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            assert(e.toString() === "TypeError: 'getOwnPropertyDescriptor' property of a Proxy's handler should be callable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let handler = {
        getOwnPropertyDescriptor: null
    };
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: false
    });
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let pDesc = Object.getOwnPropertyDescriptor(proxy, "x");
        assert(pDesc.configurable === false);
        assert(pDesc.enumerable === true);
    }
}

{
    let target = {};
    let handler = {
        getOwnPropertyDescriptor: undefined
    };
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: false
    });
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let pDesc = Object.getOwnPropertyDescriptor(proxy, "x");
        assert(pDesc.configurable === false);
        assert(pDesc.enumerable === true);
    }
}

{
    let target = {};
    let handler = { };
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: false
    });
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let pDesc = Object.getOwnPropertyDescriptor(proxy, "x");
        assert(pDesc.configurable === false);
        assert(pDesc.enumerable === true);
    }
}

{
    let target = {};
    let error = null;
    let handler = { get getOwnPropertyDescriptor() {
        error = new Error("blah");
        throw error;
    }};
    Object.defineProperty(target, "x", {
        enumerable: true,
        configurable: false
    });
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            let pDesc = Object.getOwnPropertyDescriptor(proxy, "x");
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}
