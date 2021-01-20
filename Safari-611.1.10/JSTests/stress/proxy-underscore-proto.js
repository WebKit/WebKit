function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

{
    let p = {};
    let target = {__proto__: p};
    let called = false;
    let handler = {
        get(target, key, receiver) {
            called = true;
            assert(key === "__proto__");
            return target[key];
        },

        getPrototypeOf() {
            assert(false);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.__proto__ === p);
        assert(called);
        called = false;
    }
}

{
    let p = {};
    let target = {__proto__: p};
    let called1 = false;
    let called2 = false;
    let handler = {
        get(target, key, receiver) {
            called1 = true;
            assert(key === "__proto__");
            return Reflect.get(target, key, receiver);
        },

        getPrototypeOf(...args) {
            called2 = true;
            return Reflect.getPrototypeOf(...args);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.__proto__ === p);
        assert(called1);
        assert(called2);
        called1 = false;
        called2 = false;
    }
}

{
    let p = {};
    let target = {__proto__: null};
    let called = false;
    let handler = {
        set(target, key, value, receiver) {
            called = true;
            assert(key === "__proto__");
            return target[key] = value;
        },

        setPrototypeOf() {
            assert(false);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        proxy.__proto__ = p;
        assert(proxy.__proto__ === p);
        assert(target.__proto__ === p);
        target.__proto__ = null;
        assert(called);
        called = false;
    }
}

{
    let p = {};
    let target = {__proto__: null};
    let called = false;
    let handler = {
        set(target, key, value, receiver) {
            called = true;
            assert(key === "__proto__");
            return Reflect.set(target, key, value, receiver);
        },

        setPrototypeOf() {
            assert(false);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        proxy.__proto__ = p;
        assert(proxy.__proto__ === p);
        assert(target.__proto__ === p);
        target.__proto__ = null;
        assert(called);
        called = false;
    }
}
