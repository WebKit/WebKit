function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

{
    let target = {};
    let error = null;
    let handler = {
        get setPrototypeOf() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let setters = [
            () => Reflect.setPrototypeOf(proxy, {}),
            () => Object.setPrototypeOf(proxy, {}),
            () => proxy.__proto__ = {},
        ];
        for (let set of setters) {
            let threw = false;
            try {
                set();
            } catch(e) {
                assert(e === error);
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        setPrototypeOf: function() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let setters = [
            () => Reflect.setPrototypeOf(proxy, {}),
            () => Object.setPrototypeOf(proxy, {}),
            () => proxy.__proto__ = {},
        ];
        for (let set of setters) {
            let threw = false;
            try {
                set();
            } catch(e) {
                assert(e === error);
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        setPrototypeOf: 25
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let setters = [
            () => Reflect.setPrototypeOf(proxy, {}),
            () => Object.setPrototypeOf(proxy, {}),
            () => proxy.__proto__ = {},
        ];
        for (let set of setters) {
            let threw = false;
            try {
                set();
            } catch(e) {
                assert(e.toString() === "TypeError: 'setPrototypeOf' property of a Proxy's handler should be callable");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    target.__proto__ = null;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            return true;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let setters = [
            () => Reflect.setPrototypeOf(proxy, {}),
            () => Object.setPrototypeOf(proxy, {}),
            () => proxy.__proto__ = {},
        ];
        for (let set of setters) {
            let result = set();
            assert(result);
            assert(Reflect.getPrototypeOf(target) === null);
            // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
            //assert(Reflect.getPrototypeOf(proxy) === null);
            //assert(proxy.__proto__ === null);
        }
    }
}

{
    let target = {};
    target.__proto__ = null;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            assert(theTarget === target);
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let setters = [
            () => Reflect.setPrototypeOf(proxy, obj),
            () => Object.setPrototypeOf(proxy, obj),
            () => proxy.__proto__ = obj,
        ];
        for (let set of setters) {
            let result = set();
            assert(result);
            assert(Reflect.getPrototypeOf(target) === obj);
            // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
            //assert(Reflect.getPrototypeOf(proxy) === obj);
            //assert(proxy.__proto__ === obj);
        }
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);
    let called = false;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            called = true;
            assert(theTarget === target);
            assert(value !== null);
            Reflect.setPrototypeOf(theTarget, value);
            return true;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let setters = [
            () => Reflect.setPrototypeOf(proxy, obj),
            () => Object.setPrototypeOf(proxy, obj),
            () => proxy.__proto__ = obj,
        ];
        for (let set of setters) {
            let threw = false;
            try {
                set();
            } catch(e) {
                threw = true;
                assert(called);
                called = false;
                assert(e.toString() === "TypeError: Proxy 'setPrototypeOf' trap returned true when its target is non-extensible and the new prototype value is not the same as the current prototype value. It should have returned false");
            }

            assert(threw);
            assert(Reflect.getPrototypeOf(target) === null);
            // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
            //assert(Reflect.getPrototypeOf(proxy) === null);
            //assert(proxy.__proto__ === null);
        }
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);
    let called = false;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            called = true;
            assert(theTarget === target);
            assert(value === null);
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let proto = null;
        let setters = [
            [() => Reflect.setPrototypeOf(proxy, null), true],
            [() => Object.setPrototypeOf(proxy, null), proxy],
            [() => proxy.__proto__ = null, null]
        ];
        for (let [set, expectedResult] of setters) {
            let result = set();
            assert(result === expectedResult);
            assert(called);
            called = false;
            assert(Reflect.getPrototypeOf(target) === null);
            // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
            //assert(Reflect.getPrototypeOf(proxy) === null);
            //assert(proxy.__proto__ === null);
        }
    }
}

{
    let target = {};
    let obj = {};
    target.__proto__ = obj;
    Reflect.preventExtensions(target);
    let called = false;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            called = true;
            assert(theTarget === target);
            assert(value === obj);
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let proto = null;
        let setters = [
            [() => Reflect.setPrototypeOf(proxy, obj), true],
            [() => Object.setPrototypeOf(proxy, obj), proxy],
            [() => proxy.__proto__ = obj, obj]
        ];
        for (let [set, expectedResult] of setters) {
            let result = set();
            assert(result === expectedResult);
            assert(called);
            called = false;
            assert(Reflect.getPrototypeOf(target) === obj);
            // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
            //assert(Reflect.getPrototypeOf(proxy) === null);
            //assert(proxy.__proto__ === null);
        }
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let threw = false;
        try {
            Object.setPrototypeOf(proxy, obj);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed");
        }

        assert(threw);
        assert(Reflect.getPrototypeOf(target) === null);
        // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
        //assert(Reflect.getPrototypeOf(proxy) === null);
        //assert(proxy.__proto__ === null);
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let threw = false;
        try {
            proxy.__proto__ = obj;
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed");
        }

        assert(threw);
        assert(Reflect.getPrototypeOf(target) === null);
        // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
        //assert(Reflect.getPrototypeOf(proxy) === null);
        //assert(proxy.__proto__ === null);
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);

    let called = false;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            called = true;
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.setPrototypeOf(proxy, {});
        assert(!result);
        assert(Reflect.getPrototypeOf(target) === null);

        assert(called);
        called = false;
        // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
        //assert(Reflect.getPrototypeOf(proxy) === null);
        //assert(proxy.__proto__ === null);
    }
}

{
    let target = {};
    target.__proto__ = null;
    Reflect.preventExtensions(target);

    let called = false;
    let handler = {
        setPrototypeOf: function(theTarget, value) {
            called = true;
            return Reflect.setPrototypeOf(theTarget, value);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.setPrototypeOf(proxy, null);
        assert(result);
        assert(Reflect.getPrototypeOf(target) === null);
        assert(called);
        called = false;

        // FIXME: when we implement Proxy.[[GetPrototypeOf]] this should work.
        //assert(Reflect.getPrototypeOf(proxy) === null);
        //assert(proxy.__proto__ === null);
    }
}

{
    let target = {};
    let handler = {
        setPrototypeOf: null
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let result = Reflect.setPrototypeOf(proxy, obj);
        assert(result);
        assert(Reflect.getPrototypeOf(target) === obj);
    }
}

{
    let target = {};
    let handler = {
        setPrototypeOf: undefined
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let result = Reflect.setPrototypeOf(proxy, obj);
        assert(result);
        assert(Reflect.getPrototypeOf(target) === obj);
    }
}

{
    let target = {};
    let handler = { };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let obj = {};
        let result = Reflect.setPrototypeOf(proxy, obj);
        assert(result);
        assert(Reflect.getPrototypeOf(target) === obj);
    }
}
