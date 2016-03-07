function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

{
    let target = {};
    let error = null;
    let handler = {
        get getPrototypeOf() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
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
        getPrototypeOf: function() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e === error);
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let error = null;
    let target = new Proxy({}, {
        isExtensible: function() {
            error = new Error;
            throw error;
        }
    });

    let handler = {
        getPrototypeOf: function() {
            return target.__proto__;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
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
    let handler = {
        getPrototypeOf: 25
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e.toString() === "TypeError: 'getPrototypeOf' property of a Proxy's handler should be callable.");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    let handler = {
        getPrototypeOf: function() {
            return 25;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e.toString() === "TypeError: Proxy handler's 'getPrototypeOf' trap should either return an object or null.");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    let handler = {
        getPrototypeOf: function() {
            return Symbol();
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e.toString() === "TypeError: Proxy handler's 'getPrototypeOf' trap should either return an object or null.");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    Reflect.preventExtensions(target);
    let handler = {
        getPrototypeOf: function() {
            return null;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e.toString() === "TypeError: Proxy's 'getPrototypeOf' trap for a non-extensible target should return the same value as the target's prototype.");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let notProto = {};
    let target = {};
    Reflect.preventExtensions(target);
    let handler = {
        getPrototypeOf: function() {
            return notProto;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let threw = false;
            try {
                get();
            } catch(e) {
                assert(e.toString() === "TypeError: Proxy's 'getPrototypeOf' trap for a non-extensible target should return the same value as the target's prototype.");
                threw = true;
            }
            assert(threw);
        }
    }
}

{
    let target = {};
    Reflect.preventExtensions(target);
    let called = false;
    let handler = {
        getPrototypeOf: function() {
            called = true;
            return Object.prototype;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let result = get();
            assert(result === Object.prototype);
            assert(called);
            called = false;
        }
    }
}

{
    let target = {};
    let theProto = {x: 45};
    target.__proto__ = theProto;
    Reflect.preventExtensions(target);
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return Reflect.getPrototypeOf(theTarget);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            let result = get();
            assert(result === theProto);
            assert(called);
            called = false;
        }
    }
}

{
    let target = {};
    let handler = {
        getPrototypeOf: null
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let proto = Object.prototype;
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            assert(get() === proto);
        }
    }
}

{
    let target = {};
    let proto = {};
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return proto;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            assert(get() === proto);
            assert(called);
            called = false;
        }
    }
}

{
    let target = {};
    let proto = null;
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return proto;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            assert(get() === proto);
            assert(called);
            called = false;
        }
    }
}

{
    let target = {};
    let proto = null;
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return proto;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let getters = [
            () => Reflect.getPrototypeOf(proxy),
            () => Object.getPrototypeOf(proxy),
            () => proxy.__proto__,
        ];
        for (let get of getters) {
            assert(get() === proto);
            assert(called);
            called = false;
        }
    }
}

{
    let target = {};
    let proto = null;
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return proto;
        },
        has: function() {
            return false;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let proto = null;
    let called = false;
    let handler = {
        getPrototypeOf: function(theTarget) {
            called = true;
            return proto;
        },
        has: function() {
            return true;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = "x" in proxy;
        assert(!called);
    }
}
